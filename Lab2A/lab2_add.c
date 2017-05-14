#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>

int threadNum = 1;
long itNum = 1;
long long counter = 0;

int opt_yield = 0;

char sync_opt = 0;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER; 
int lock = 0;

void add (long long *pointer, long long value) {
	long long sum = *pointer + value;
	if (opt_yield)
		sched_yield();
	*pointer = sum;
}

void sync_add(int val) {
	int i = 0;
	for (i = 0; i < itNum; i++) {
		switch(sync_opt) {
			case 'm':
			{
				pthread_mutex_lock(&count_mutex);
				add(&counter, val);
				pthread_mutex_unlock(&count_mutex);
				break;
			}
			case 's':
			{
				while(__sync_lock_test_and_set(&lock, 1));
				add(&counter, val);
				__sync_lock_release(&lock);
				break;
			}
			case 'c':
			{
				long long old, new;
				do {
					old = counter;
					new = old + val;
					if (opt_yield)
						sched_yield();
				} while (__sync_val_compare_and_swap(&counter, old, new) != old);
				break;
			}
			default:
				fprintf(stderr, "Error: sync_add failed.\n");
				exit(2);
		}
	}
}

void* thread_add() {
	long a;
	long s;
	if (sync_opt == 0) {
		for (a = 0; a < itNum; a++) {
			add(&counter, 1);
		}
		for (s = 0; s < itNum; s++) {
			add(&counter, -1);
		}
	}
	else {
		sync_add(1);
		sync_add(-1);
	}
	return NULL;
}

int main(int argc, char **argv) {
	int ret = 0;
	char testopt[15] = "add-";

	if (argc < 3) {
		fprintf(stderr, "Invalid argument. Usage: lab2_add --iterations=# --threads=#\n" );
		exit(1);
	}

	static struct option long_options[] = {
		{"threads", required_argument, NULL, 't'},
		{"iterations", required_argument, NULL, 'i'},
		{"yield", no_argument, NULL, 'y'},
		{"sync", required_argument, NULL, 's'},
		{0,0,0,0}
	};

	while(1) {
		ret = getopt_long(argc, argv, "t:i:ys:", long_options, NULL);

		if (ret == -1) {
			break;
		}

		switch(ret)
		{
			case 't':
			{
				threadNum = atoi(optarg);
				break;
			}
			case 'i':
			{
				itNum = atoi(optarg);
				break;
			}
			case 'y':
			{
				opt_yield = 1;
				break;
			}
			case 's':
			{
				sync_opt = optarg[0];
				break;
			}
			default:
			{
				fprintf(stderr, "Invalid argument. Usage: ./lab2_add --iterations=# --threads=# --yield --sync=protection\n" );
				exit(1);
			}
		}
	}

	//test options
	if (opt_yield) {
		strcat(testopt, "yield-");
	}

	if (sync_opt == 0) {
		strcat(testopt, "none");
	}
	else{
		if ((sync_opt != 'm') && (sync_opt != 's') && (sync_opt != 'c'))
		{
			fprintf(stderr, "Invalid argument. Usage: lab2_add --iterations=# --threads=# --yield --sync=protection\n" );
			exit(1);
		}
		strcat(testopt, &sync_opt);
	}

	
	struct timespec start_time;
	struct timespec end_time;
	long long ns;

	//notes the starting time
	if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1)
	{
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}

	//create threads
	pthread_t thread[threadNum];
	int rc;
	long t;

	for(t = 0; t < threadNum; t++) {
		rc = pthread_create(&thread[t], NULL, thread_add, NULL);
		if (rc) {
			fprintf(stderr, "pthread_create() %s\n", strerror(errno));
			exit(1);
		}
	}

	//join threads, wait for all threads to complete
	for(t = 0; t < threadNum; t++) {
		rc = pthread_join(thread[t], NULL);
		if (rc) {
			fprintf(stderr, "pthread_join() %s\n", strerror(errno));
			exit(1);	
		}
	}

	//note the ending time for the run
	if (clock_gettime(CLOCK_MONOTONIC, &end_time))
	{
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}

	//capture the time in nanoseconds
	ns = end_time.tv_sec - start_time.tv_sec;
	ns *= 1000000000;	//convert the seconds part into nanoseconds
	ns += end_time.tv_nsec;
	ns -= start_time.tv_nsec;

	//print to stdout a CSV
	long long opt_perform = threadNum*itNum*2;
	printf("%s,%d,%d,%lld,%lld,%lld,%lld\n", testopt, threadNum, itNum, opt_perform, ns, ns/opt_perform, counter);
	exit(0);

}