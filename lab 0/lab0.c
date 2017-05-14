//NAME: Yun Xu
//EMIAL: x_one_u@yahoo.com
//ID: 304635157

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
//for open(2)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//for close(2) & dup(2) & read(2) & write(2)
#include <unistd.h>
//for signal(2)
#include <signal.h>
//for strerror
#include <errno.h>
#include <string.h>

//segfault signal handler
void sig_handler (int signum) {
	if (signum == SIGSEGV) {
		fprintf(stderr, "Error Caught: Segmentation Fault.\n");
		exit(4);
	}
}

int main(int argc, char **argv) {

	static struct option long_options[] =
	{
		{"input", required_argument, NULL, 'i'},
		{"output", required_argument, NULL, 'o'},
		{"segfault", no_argument, NULL, 's'},
		{"catch", no_argument, NULL, 'c'},
		{0,0,0,0}
	};

	int ret = 0;
	char *ifile = NULL;
	char *ofile = NULL;
	char *segfault = NULL;
	int segFlag = 0;
	int catchFlag = 0;
	//set correct flag/string corresponding to the option
	while (1) {
		ret = getopt_long(argc, argv, "i:o:sc", long_options, NULL);
		if (ret == -1) {
			break;
		}
		//process all arguments and store the results invariables
		switch(ret) {
			case 'i':
			{
				ifile = optarg;
				break;
			}
			case 'o':
			{
				ofile = optarg;
				break;
			}
			case 's':
			{
				segFlag = 1;
				break;
			}
			case 'c':
			{
				catchFlag = 1;
				break;
			}
			default:
			{
				fprintf(stderr, "Invalid arguments. Correct usage: ./lab0 --input=filename --output=filename --segfault --catch.\n");
				exit(1);
			}
		}
	}

	int ifd = 0;
	int ofd = 1;

	//check which options were specificed and carry action in the correct order

	//input redirection
	if (ifile != NULL) {
		ifd = open(ifile, O_RDONLY);
		if (ifd >= 0) {
			close (0);
			dup(ifd);
			close(ifd);
		}
		else {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(2);
		}
	}

	//output redirection
	if (ofile != NULL) {
		ofd = creat(ofile, 0666);
		if (ofd >= 0) {
			close(1);
			dup(ofd);
			close(ofd);
		}
		else {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(3);
		}
	}

	//use signal to register SIGSEGV handler
	if (catchFlag == 1) {
		signal(SIGSEGV, sig_handler);
	}

	//forced segmentation fault
	if (segFlag == 1) {
		char derefS = *segfault;
	}

	//read and write
	char buffer;
	while (read(0, &buffer, 1)) {
		write(1, &buffer, 1);
	}

	exit(0);

}