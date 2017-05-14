#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include "SortedList.h"

int threadNum = 1;
long itNum = 1;
int opt_yield = 0;
char sync_opt = 0;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER; 
int lock = 0;

SortedList_t *list;
SortedListElement_t **Elem_list;
int elemNum = 0;

long listLen = 0;

void signal_handler(int signum) {
	if (signum == SIGSEGV) {
		fprintf(stderr, "Error: Catch Segmentation Fault.\n");
		free(Elem_list);
		exit(2);
	}
}

//generate random key, reference from stackoverflow
char *randString () {
	int keylen = rand() % 10 + 1;
	char *randKey = malloc(sizeof(char)*(keylen+1));
	if (randKey == NULL) {
		fprintf(stderr, "key malloc %s\n", strerror(errno));
		exit(1);
	}
	int j;
	for (j = 0; j < keylen; j++) {	
		randKey[j] = (char) rand() % 72 + '0';
	}
	randKey[keylen] = '\0';

	return randKey;
}

void* thread_routine(void *arg) {
	int tid = *(int*) arg;
	int i, j;
	int start = tid*itNum;
	int end = (tid+1)*itNum;

	//insert element
	for (i = start; i < end; i++) {
		if (sync_opt == 0)
			SortedList_insert(list, Elem_list[i]);
		else {
			switch(sync_opt) 
			{
				case 'm':
				{
					pthread_mutex_lock(&list_mutex);
					SortedList_insert(list, Elem_list[i]);
					pthread_mutex_unlock(&list_mutex);
					break;
				}
				case 's':
				{
					while(__sync_lock_test_and_set(&lock, 1));
					SortedList_insert(list, Elem_list[i]);
					__sync_lock_release(&lock);
					break;
				}
				default:
					fprintf(stderr, "Error: sync insert failed.\n");
					exit(2);
			}
		}
		//fprintf(stderr, "insert: %s  i: %d\n", Elem_list[i]->key, i);
	}

	//gets the list length
	if (sync_opt == 0)
		listLen = SortedList_length(list);
	else {
		switch(sync_opt) 
		{
			case 'm':
			{
				pthread_mutex_lock(&list_mutex);
				listLen = SortedList_length(list);
				pthread_mutex_unlock(&list_mutex);
				break;
			}
			case 's':
			{
				while(__sync_lock_test_and_set(&lock, 1));
				listLen = SortedList_length(list);
				__sync_lock_release(&lock);
				break;
			}
			default:
				fprintf(stderr, "Error: sync length failed.\n");
				exit(2);
		}
	}
	if (listLen < 0) {
		fprintf(stderr, "Error: Failed finding list length. List is corrupted.\n");
		exit(2);
	}

	//look up and delete
	SortedListElement_t *cur;
	int rc;
	for (j = start; j < end; j++) {
		//look up and delete element
		if (sync_opt == 0) {
			cur = SortedList_lookup(list, Elem_list[j]->key);
			rc = SortedList_delete(cur);
		}
		else {
			switch(sync_opt) 
			{
				case 'm':
				{
					pthread_mutex_lock(&list_mutex);
					cur = SortedList_lookup(list, Elem_list[j]->key);
					rc = SortedList_delete(cur);
					pthread_mutex_unlock(&list_mutex);
					break;
				}
				case 's':
				{
					while(__sync_lock_test_and_set(&lock, 1));
					cur = SortedList_lookup(list, Elem_list[j]->key);
					rc = SortedList_delete(cur);
					__sync_lock_release(&lock);
					break;
				}
				default:
					fprintf(stderr, "Error: sync delete failed.\n");
					exit(2);
			}
		}
		if (rc != 0) {
			fprintf(stderr, "Error: Failed deleting the list element %s. List is corrupted.\n", Elem_list[j]->key);
			exit(2);
		}
	}

	//find the final listLen
	//listLen = SortedList_length(list);
	//fprintf(stderr, "%ld\n", listLen);

	return NULL;
}

int main(int argc, char **argv) {
	int ret = 0;
	char testopt[20] = "list-";
	//char *yield_opt = NULL;

	static struct option long_options[] = {
		{"threads", required_argument, NULL, 't'},
		{"iterations", required_argument, NULL, 'i'},
		{"yield", required_argument, NULL, 'y'},
		{"sync", required_argument, NULL, 's'},
		{0,0,0,0}
	};

	while(1) {
		ret = getopt_long(argc, argv, "t:i:y:s:", long_options, NULL);

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
				int len = strlen(optarg);
				if (len > 3) {
					fprintf(stderr, "Invalid argument. Usage: ./lab2_list --threads=# --iterations=# --yield=idl --sync=ms\n");
					exit(1);
				}

				for (i = 0; i < len; i++) {
					switch(optarg[i])
					{
						case 'i':
							opt_yield |= INSERT_YIELD;
							break;
						case 'd':
							opt_yield |= DELETE_YIELD;
							break;
						case 'l':
							opt_yield |= LOOKUP_YIELD;
							break;
						default:
							fprintf(stderr, "Invalid argument. Usage: ./lab2_list --threads=# --iterations=# --yield=idl --sync=ms\n");
							exit(1);
					}
				}

				break;
			}
			case 's':
			{
				sync_opt = optarg[0];
				break;
			}
			default:
			{
				fprintf(stderr, "Invalid argument. Usage: ./lab2_list --threads=# --iterations=# --yield=idl --sync=ms\n");
				exit(1);
			}
		}
	}

	if (opt_yield == 0) {
		strcat(testopt, "none-");
	}
	else {
		if (opt_yield & INSERT_YIELD)
			strcat(testopt, "i");
		if (opt_yield & DELETE_YIELD)
			strcat(testopt, "d");
		if (opt_yield & LOOKUP_YIELD)
			strcat(testopt, "l");
		strcat(testopt, "-");
	}

	/*//yield part of output string 
	if (yield_opt != NULL) {
		int len = strlen(yield_opt);
		if (len > 3) {
			fprintf(stderr, "Invalid argument. Usage: ./lab2_list --threads=# --iterations=# --yield=idl --sync=ms\n");
			exit(1);
		}
		int i;
		for (i = 0; i < len; i++) {
			switch(yield_opt[i])
			{
				case 'i':
					opt_yield |= INSERT_YIELD;
					break;
				case 'd':
					opt_yield |= DELETE_YIELD;
					break;
				case 'l':
					opt_yield |= LOOKUP_YIELD;
					break;
				default:
					fprintf(stderr, "Invalid argument. Usage: ./lab2_list --threads=# --iterations=# --yield=idl --sync=ms\n");
					exit(1);
			}
		}
		strcat(testopt, yield_opt);
		strcat(testopt, "-");
	}
	else 
		strcat(testopt, "none-");*//

	//sync part of output string
	if (sync_opt != 0) {
		if ((sync_opt != 'm') && (sync_opt != 's'))
		{
			fprintf(stderr, "Invalid argument. Usage: ./lab2_list --threads=# --iterations=# --yield=idl --sync=ms\n" );
			exit(1);
		}
		strcat(testopt, &sync_opt);
	}
	else 
		strcat(testopt, "none");

	//listen to see if there's seg fault
	signal(SIGSEGV, signal_handler);

	//initialize an empty list
	list = malloc(sizeof(SortedList_t));
	if (list == NULL) {
		fprintf(stderr, "list malloc %s\n", strerror(errno));
		exit(1);
	}
	list->prev = list;
	list->next = list;
	list->key = NULL;

	//create and initialize the required number of list elements before start time
	elemNum = threadNum*itNum;
	Elem_list = malloc(sizeof(SortedListElement_t*)*elemNum);
	if (Elem_list == NULL) {
		fprintf(stderr, "list malloc%s\n", strerror(errno));
		exit(1);
	}
	long i;	
	for (i = 0; i < elemNum; i++) {
		Elem_list[i] = malloc(sizeof(SortedListElement_t));
		if (Elem_list[i] == NULL) {
			fprintf(stderr, "element malloc %s\n", strerror(errno));
			exit(1);
		}
		Elem_list[i]->key = randString();
		Elem_list[i]->prev = NULL;
		Elem_list[i]->next = NULL;
	}

	struct timespec start_time;
	struct timespec end_time;
	long long ns;

	//notes the starting time
	if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1)
	{
		fprintf(stderr, "clock start time %s\n", strerror(errno));
		exit(1);
	}

	//create thread
	pthread_t threads[threadNum];
	int tid[threadNum];
	int rc;
	long t;
	for (t = 0; t < threadNum; t++) {	
		tid[t] = t;
		rc = pthread_create(&threads[t], NULL, thread_routine, (void*)(tid+t));
		if (rc) {
			fprintf(stderr, "pthread_create() %s\n", strerror(errno));
			free(Elem_list);
			exit(1);
		}
	}

	//join thread
	for (t = 0; t < threadNum; t++) {	
		rc = pthread_join(threads[t], NULL);
		if (rc) {
			fprintf(stderr, "pthread_join() %s\n", strerror(errno));
			free(Elem_list);
			exit(1);
		}
	}

	//notes the end time
	if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1)
	{
		fprintf(stderr, "clock end time%s\n", strerror(errno));
		exit(1);
	}

	//checks the length of the list to confirm that it is zero
	listLen = SortedList_length(list);
	if (listLen != 0) {
		fprintf(stderr, "Error: List length = %d\n", listLen);
		exit(2);
	}

	//capture the time in nanoseconds
	ns = end_time.tv_sec - start_time.tv_sec;
	ns *= 1000000000;	//convert the seconds part into nanoseconds
	ns += end_time.tv_nsec;
	ns -= start_time.tv_nsec;

	//print to stdout a CSV
	long long opt_perform = threadNum*itNum*3;
	int listNum = 1;
	printf("%s,%d,%d,%d,%lld,%lld,%lld\n", testopt, threadNum, itNum, listNum, opt_perform, ns, ns/opt_perform);
	exit(0);

}
