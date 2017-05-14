#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PANDA 'p'
#define SLEEP 's'
#define CATCH 'c'
#define SHOWBUG 'b'

void a_tiny_handler (int signum) {
	char my_message[35] = "signal received, now quitting...\n";
	write (1, my_message, 34);
	//fprintf(stderr, "Signal number : %d received, now quitting...\n", signum);
	//it's not safe to use fprintf(), why? hint async-signal-safe and malloc()
	exit(1);
}

int main(int argc, char *argv[])
{
	//long option
	struct option long_options[] = 
	{
		{"pandaexpress", no_argument, NULL, PANDA},
		{"sleep", required_argument, NULL, SLEEP},
		{"catch", required_aregument, NULL, CATCH},
		{"showbug", no_argument, NULL, SHOWBUG},
		{0,0,0,0}
	};

	int ret = 0;
	fprintf(stderr, "argv[] ");
	for (int i = 0; i < argc; ++i) {
		fprintf(stderr, "%s, ", argv[i]);
	}

	fprintf(stderr, "\n");

	while (1) {
		ret = getopt_long(argc, argv, "", long_options, NULL);
		fprintf(stderr, "argv[]: ");
		for (int i = 0; i < argc; ++i) {
			fprintf(stderr, "%s, ", argv[i]);
		}
		fprintf(stderr, "\n");
		if (ret == -1) {
			break;
		}
		fprintf(stderr, "ret: %c, optind: %d, optarg: %s\n", ret, optind, optarg);
		switch(ret) {
			case PANDA:
			{
				fprintf(stdout, "Panda Express is not real Chinese food!!!\n");
				break;
			}
			case SLEEP:
			{
				int time = atoi(optarg);
				fprintf(stdout, "Now sleeping %d seconds.\n", time);
				sleep(time);
				break;
			}
			case CATCH:
			{
				int signum = atoi(optarg);
				fprintf(stderr, "signal()\n");
				signal(signum, a_tiny_handler);
				break;
			}
			case SHOWBUG:
			{
				dup(100);
				perror("Here is the error message Zhaoxing wants to show");
				break;
			}
			default:
			{
				fprintf(stderr, "Oops...\n");
				exit(1);
			}
		}
	}

	fprintf(stdout, "Knowledge crowns those who seek her.\n");
	return 0;
}