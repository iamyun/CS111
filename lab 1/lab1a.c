#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <poll.h>
#include <sys/wait.h>
#include <signal.h>

struct termios saved_attr;
int buff_size = 255;
char retn[2] = {'\r', '\n'};

int to_child_pipe[2];
int from_child_pipe[2];
pid_t child_pid = -1;

int sFlag = 0;	//shell flag
int check = 0; //check for syserror

//restore the original terminal attributes
void checkSysErr (int checkErr, char err[]) {
	if (checkErr == -1) {
		fprintf(stderr, "%s\n", strerror(errno));
		fprintf(stderr, "%s\n", err);
		exit(1);
	}
}

void reset_input_mode (void) {
	check = tcsetattr (STDIN_FILENO, TCSANOW, &saved_attr);
	checkSysErr(check, "reset");

	if (sFlag) {
		int status = 0;
		check = waitpid(child_pid, &status, 0);
		checkSysErr(check, "waitpid");
		fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WEXITSTATUS(status), WTERMSIG(status));
	}
}

void set_input_mode (void) {
	struct termios tattr;
	char *name;

	//check if stdin is a terminal
	if (!isatty(STDIN_FILENO)) {
		fprintf(stderr, "NOT INPUT TERMINAL\n");
		exit(1);
	}

	//save orignal terminal attributes
	check = tcgetattr(STDIN_FILENO, &saved_attr);
	checkSysErr(check, "tcgetattr");
	check = atexit(reset_input_mode);
	checkSysErr(check, "atexit");

	//set to non-canonical input mode with no echo
	check = tcgetattr(STDIN_FILENO, &tattr);
	checkSysErr(check, "tcgetattr");
	tattr.c_iflag = ISTRIP;
	tattr.c_oflag = 0;
	tattr.c_lflag = 0;
	check = tcsetattr(STDIN_FILENO, TCSANOW, &tattr);
	checkSysErr(check, "tcsetattr");
}

void signal_handler(int signum) {
	if (signum == SIGPIPE) {
		fprintf(stderr, "Caught SIGPIPE Error.\n");
		exit(1);
	}
}

/*void shutdown() {
	check = close(to_child_pipe[1]);
	checkSysErr(check);
	int shellCt = 0;
	char buff[buff_size];
	shellCt = read(from_child_pipe[0], buff, buff_size);
	checkSysErr(shellCt);
	
	if (shellCt <= 0) {
		exit(0);
	}

	for (int i = 0; i < shellCt; i++) 
	{		
		if (buff[i] == '\r' || buff[i] == '\n') {
			check = write(STDOUT_FILENO, retn, 2);
			checkSysErr(check);
		}
		else {
			check = write(STDOUT_FILENO, buff+i, 1);
			checkSysErr(check);
		}
	}	
	exit(0);
}*/

int main (int argc, char **argv) {
	char buffer[buff_size];
	int count = 0;
	int i = 0;

	int ret = 0;
	
	//add the shell option
	static struct option long_options[] = {
		{"shell", no_argument, NULL, 's'},
		{0,0,0,0}
	};

	while (1) {
		ret = getopt_long(argc, argv, "s", long_options, NULL);
		if (ret == -1) {
			break;
		}

		switch (ret) 
		{
			case 's':
			{
				signal(SIGPIPE, signal_handler);
				sFlag = 1;	//set shell flag
				break;
			}
			default:
				break;
		}
	}

	//set non-canonical input mode
	set_input_mode();
	
	//create the to and from pipe, since pipes are unidirectional
	if (pipe(to_child_pipe) == -1) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}

	if (pipe(from_child_pipe) == -1) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}

	if (sFlag == 1) {
		child_pid = fork();	//fork

		if (child_pid > 0) { //parent process
			check = close(to_child_pipe[0]);
			checkSysErr(check, "close to child 0");
			check = close(from_child_pipe[1]);
			checkSysErr(check, "close from child 1");

			struct pollfd fds[2];
			int timeout = 0;
			int pRet = 0;

			fds[0].fd = STDIN_FILENO;	//reading from keyboard
			fds[1].fd = from_child_pipe[0];	//reading from shell
			fds[0].events = POLLIN | POLLHUP | POLLERR;
			fds[1].events = POLLIN | POLLHUP | POLLERR;

			while (1) {
				pRet = poll(fds, 2, timeout);
				checkSysErr(pRet, "poll");

				if (pRet > 0) {
					//if received input from keyboard, write to STDOUT and shell
					if (fds[0].revents & POLLIN) {
						count = read (STDIN_FILENO, buffer, buff_size);
						checkSysErr(count, "fds0 read");

						for (i = 0; i < count; i++) 
						{
							char c = buffer[i];

							if (c == 0x03) { //C-c
								check = kill(child_pid, SIGINT);
								checkSysErr(check, "kill ^C");
								exit(0);
							}	
							else if (c == '\004') {	//C-d
								//shutdown();
								check = close(to_child_pipe[1]);
								checkSysErr(check, "shell ^D");
							}
							else if (c == 0x0A || c == 0x0D) {
								check = write(STDOUT_FILENO, retn, 2);
								checkSysErr(check, "write crlf to stdout");
								//send to shell as <lf>
								buffer[i] = '\n';
								check = write(to_child_pipe[1], buffer+i, count);
								checkSysErr(check, "write lf to child 1");
							}
							else {
								check = write(STDOUT_FILENO, &c, 1);
								checkSysErr(check, "write to stdout");
								check = write(to_child_pipe[1], buffer+i, count);
								checkSysErr(check, "write to child 1");
							}
						}
						//write to shell
						/*check = write(to_child_pipe[1], buffer, count);
						checkSysErr(check);*/
					}

					//if received input from the shell, write to STDOUT
					if (fds[1].revents & POLLIN) {
						count = read (from_child_pipe[0], buffer, buff_size);
						//checkSysErr(count);
						if (count <= 0) {
							//fprintf(stderr, "%d\n", count);
							exit(0);
						}
						else {
							for (i = 0; i < count; i++) 
							{
								char c = buffer[i];						
								
								if (c == 0x0D || c == 0x0A) {
									check = write(STDOUT_FILENO, retn, 2);
									checkSysErr(check, "shell write crlf");
								}
								else {
									check = write(STDOUT_FILENO, &c, 1);
									checkSysErr(check, "shell write");
								}
							}
						}
					}
					//if received POLLHUP and POLLERR
					if ((fds[0].revents) & (POLLHUP | POLLERR)) {
						check = close(to_child_pipe[1]);
						checkSysErr(check, "close to child 1 hup/err");
						check = close(from_child_pipe[0]);
						checkSysErr(check, "close from child 0");
					}

					if ((fds[1].revents) & (POLLHUP | POLLERR)) {
						check = close(to_child_pipe[1]);
						checkSysErr(check, "close to child 1 hup/err");
						check = close(from_child_pipe[0]);
						checkSysErr(check, "close from child 0");
					}
				}
			}	
		}
		else if (child_pid == 0) { //child process
			check = close(to_child_pipe[1]);
			checkSysErr(check, "child p close to child 1");
			check = close(from_child_pipe[0]);
			checkSysErr(check, "close from child 0");
			check = dup2(to_child_pipe[0], 0);
			checkSysErr(check, "dup to child 0");
			check = dup2(from_child_pipe[1], 1);
			checkSysErr(check, "dup from child, 1");
			check = dup2(from_child_pipe[1], 2);
			checkSysErr(check, "dup from child 1");
			check = close(to_child_pipe[0]);
			checkSysErr(check, "close to child 0");
			check = close(from_child_pipe[1]);
			checkSysErr(check, "close from child 1");

			char *execvp_argv[2];
			char execvp_filename[] = "/bin/bash";
			execvp_argv[0] = execvp_filename;
			execvp_argv[1] = NULL;
			if (execvp(execvp_filename, execvp_argv) == -1) {
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1);
			}
		}
		else {	//pipe failed
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
	}
	else {
		while (1) {
			count = read (STDIN_FILENO, buffer, buff_size);
			checkSysErr(count, "read regular");
			for (i = 0; i < count; i++) 
			{
				if (buffer[i] == '\004') {	//C-d
					exit(0);
				}

				if (buffer[i] == '\r' || buffer[i] == '\n') {
					check = write(STDOUT_FILENO, retn, 2);
					checkSysErr(check, "write crlf regular");
				}
				else {
					write(STDOUT_FILENO, buffer+i, 1);
					checkSysErr(check, "write regular");
				}
			}
		}
	}

	return 0;
}