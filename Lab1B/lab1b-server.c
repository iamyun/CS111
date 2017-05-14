#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <mcrypt.h>

int buff_size = 256;
char retn[2] = {'\r', '\n'};

int to_child_pipe[2];
int from_child_pipe[2];
pid_t child_pid = -1;
MCRYPT encr_fd, decr_fd;

int eFlag = 0;

int sockfd, newsockfd;

void shut_down (int eCode) {
	int status = 0;
	waitpid(child_pid, &status, 0);
	fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));

	close(newsockfd);
	close(sockfd);

	if(eFlag) {
		mcrypt_generic_deinit(encr_fd);
		mcrypt_module_close(encr_fd);
		mcrypt_generic_deinit(decr_fd);
		mcrypt_module_close(decr_fd);
	}

	if (eCode) {
		exit(1);
	}
	else {
		exit(0);
	}
}

void signal_handler (int signum) {
	if (signum == SIGPIPE) {
		fprintf(stderr, "Caught SIGPIPE.\n");
		shut_down(0);
	}
}

int main(int argc, char **argv) {
	int portno, clilen;
	struct sockaddr_in serv_addr, cli_addr;
	char buffer[buff_size];
	int ret = 0;
	int i = 0;
	int count = 0;

	char* key;
	char* keyfile = NULL;
	int keylen;

	if (argc < 2) {
		fprintf(stderr, "Invalid argument. Usage: ./lab1b_server --port=portno --encrypt=filename\n");
		exit(1);
	}

	static struct option long_options[] = {
		{"port", required_argument, NULL, 'p'},
		{"encrypt", required_argument, NULL, 'e'},
		{0,0,0,0}
	};

	while (1) {
		ret = getopt_long(argc, argv, "p:e:", long_options, NULL);

		if (ret == -1) {
			break;
		}

		switch(ret)
		{
			case 'p':
			{
				portno = atoi(optarg);
				break;
			}
			case 'e':
			{
				eFlag = 1;
				keyfile = optarg;	//get filename of key
				break;
			}
			default:
			{
				fprintf(stderr, "Invalid argument. Usage: ./lab1b_server --port=portno --encrypt=filename\n");
				exit(1);
			}
		}
	}

	//call to socket() function
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		shut_down(1);
	}

	//Initialize socket structure
	memset((char*)&serv_addr, 0, sizeof(serv_addr));	//set all the socket structures with null values
	//portno = 5001;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	//bind the host address using bind() call
	if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		shut_down(1);
	}

	//listen for the clients, here process will go in sleep mode and will wait 
	//for the incoming connection
	listen(sockfd, 5);
	clilen = sizeof(cli_addr);

	//accept actual connection from the client
	newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
	if (newsockfd < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		shut_down(1);
	}

	//encrypt
	if (eFlag) {
		//get data about my.key
		struct stat key_stat;
		int keyfd = 0;
		if (keyfile != NULL) {
			keyfd = open(keyfile, O_RDONLY);
			if (keyfd < 0) {
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1);
			}
		}
		if (fstat(keyfd, &key_stat) < 0) {
			fprintf(stderr, "key stat %s\n", strerror(errno));
			shut_down(1);
		}
		keylen = key_stat.st_size;
		key = (char*)malloc(keylen*sizeof(char));
		if (read(keyfd, key, keylen) < 0) {
			fprintf(stderr, "key %s\n", strerror(errno));
			shut_down(1);
		}

		//initialize encryption and decryption
		encr_fd = mcrypt_module_open("twofish", NULL, "cfb", NULL);
		if (encr_fd == MCRYPT_FAILED) {
			fprintf(stderr, "ERROR: Failed opening module\n");
			shut_down(1);
		}
		if (mcrypt_generic_init(encr_fd, key, keylen, NULL) < 0) {
			fprintf(stderr, "ERROR: Failed initializing encryption buffer\n");
			shut_down(1);
		}

		decr_fd = mcrypt_module_open("twofish", NULL, "cfb", NULL);
		if (decr_fd == MCRYPT_FAILED) {
			fprintf(stderr, "ERROR: Failed opening module\n");
			shut_down(1);
		}
		if(mcrypt_generic_init(decr_fd, key, keylen, NULL) < 0) {
			fprintf(stderr, "ERROR: Failed initializing decryption buffer\n");
			shut_down(1);
		}
	}	


	//create the to and from pipe
	if (pipe(to_child_pipe) == -1) {
		fprintf(stderr, "%s\n", strerror(errno));
		shut_down(1);
	}

	if (pipe(from_child_pipe) == -1) {
		fprintf(stderr, "%s\n", strerror(errno));
		shut_down(1);
	}

	//fork
	child_pid = fork();
	signal(SIGPIPE, signal_handler);

	if (child_pid > 0) {	//parent process 
		close(to_child_pipe[0]);
		close(from_child_pipe[1]);

		struct pollfd fds[2];
		int timeout = 0;
		int pRet = 0;

		fds[0].fd = newsockfd;	//reading from socket
		fds[1].fd = from_child_pipe[0];	//reading from shell
		fds[0].events = POLLIN | POLLHUP | POLLERR;
		fds[1].events = POLLIN | POLLHUP | POLLERR;

		while (1) {
			pRet = poll(fds, 2, timeout);

			if (pRet < 0) {
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1);
			}
			else if (pRet > 0) {
				//reading from socket, just write everything to shell
				if (fds[0].revents & POLLIN) {
					memset(buffer, 0, 256);
					count = read(newsockfd, buffer, buff_size);

					if (count <= 0) {
						kill(child_pid, SIGTERM);
					}
					else 
					{
						//decrypt before sending to shell
						if (eFlag) {
							if (mdecrypt_generic(encr_fd, buffer, count) != 0) {
								fprintf(stderr, "ERROR: encryption did not succeed.\n");
								shut_down(1);
							}
						}

						for (i = 0; i < count; i++) {
							char c = buffer[i];
							if (c == 0x03) {	//C-c
								kill(child_pid, SIGINT);
								//shut_down(0);
							}
							else if (c == 0x04) {	//C-d
								close(to_child_pipe[1]);
							}
							else {
								write(to_child_pipe[1], &c, 1);
							}
						}
					}
				}

				//reading from shell, write to socket
				if (fds[1].revents & POLLIN) {
					memset(buffer, 0, 256);
					count = read(from_child_pipe[0], buffer, buff_size);

					if (count <= 0) {
						shut_down(0);
					}

					//encrypt before sending to socket
					if (eFlag) {
						if (mcrypt_generic(encr_fd, buffer, count) != 0) {
							fprintf(stderr, "ERROR: encryption did not succeed.\n");
							shut_down(1);
						}
					}

					write(newsockfd, buffer, count);
				}

				//if receive POLLHUP or POLLERR
				if ((fds[0].revents) & (POLLHUP | POLLERR)) {
					//close(to_child_pipe[1]);
					close(from_child_pipe[0]);
					shut_down(0);
				}

				if ((fds[1].revents) & (POLLHUP | POLLERR)) {
					//close(to_child_pipe[1]);
					close(from_child_pipe[0]);
					shut_down(0);
				}
			}
		}
	}
	else if (child_pid == 0) {	//child process
		close(to_child_pipe[1]);
		close(from_child_pipe[0]);
		//redirect the shell process's stdin/stdout/stderr to the appropriate pipe ends
		dup2(to_child_pipe[0], 0);
		dup2(from_child_pipe[1], 1);
		dup2(from_child_pipe[1], 2);
		close(to_child_pipe[0]);
		close(from_child_pipe[1]);

		char *execvp_argv[2];
		char execvp_filename[] = "/bin/bash";
		execvp_argv[0] = execvp_filename;
		execvp_argv[1] = NULL;
		if (execvp(execvp_filename, execvp_argv) == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			shut_down(1);
		}
	}
	else {//pipe failed
		fprintf(stderr, "%s\n", strerror(errno));
		shut_down(1);
	}
}