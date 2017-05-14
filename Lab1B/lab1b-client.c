#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termios.h>
#include <errno.h>
#include <getopt.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <mcrypt.h>

struct termios saved_attr;
int buff_size = 256;
char retn[2] = {'\r', '\n'};

int sockfd, logfd;
int eFlag = 0;
int lFlag = 0;

MCRYPT encr_fd, decr_fd;

void reset_input_mode(void) {
	tcsetattr(STDIN_FILENO, TCSANOW, &saved_attr);

	close(sockfd);

	//terminates encryption, clears all buffers, closes the modules
	if(eFlag) {
		mcrypt_generic_deinit(encr_fd);
		mcrypt_module_close(encr_fd);
		mcrypt_generic_deinit(decr_fd);
		mcrypt_module_close(decr_fd);
	}
}

void set_input_mode(void) {
	struct termios tattr;
	char *name;

	//check if stdin is a terminal
	if (!isatty(STDIN_FILENO)) {
		fprintf(stderr, "NOT INPUT TERMINAL\n");
		exit(1);
	}

	//save original terminal attributes
	tcgetattr(STDIN_FILENO, &saved_attr);
	atexit(reset_input_mode);

	tcgetattr(STDIN_FILENO, &tattr);
	tattr.c_iflag = ISTRIP;
	tattr.c_oflag = 0;
	tattr.c_lflag = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &tattr);	
}

void logging (char* buffer, int num_bytes, int toSock) {
	char sent_buff[6] = "SENT ";
	char rec_buff[10] = "RECEIVED ";
	char num[5];	//max 3 digit for num_bytes + space + '\0'
	sprintf(num, "%d ", num_bytes);
	char byte_byff[8] = "bytes: ";
	//different starting line for sent and received
	if (toSock) {
		int head_len = 5+strlen(num)+8;
		char *log_head = malloc(head_len);
		strcpy(log_head, sent_buff);
		strcat(log_head, num);
		strcat(log_head, byte_byff);
		write(logfd, log_head, head_len);
	}
	else {
		int head_len = 9+strlen(num)+8;
		char *log_head = malloc(head_len);
		strcpy(log_head, rec_buff);
		strcat(log_head, num);
		strcat(log_head, byte_byff);
		write(logfd, log_head, head_len);
	}
	write(logfd, buffer, num_bytes);
	write(logfd, "\n", 1);
}

int main(int argc, char **argv) {
	int sockfd, portno;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	char buffer[buff_size];
	int ret = 0;
	int count = 0;
	int i = 0;
	char *logfile = NULL;

	char* key;
	char* keyfile = NULL;
	int keylen;

	if (argc < 2) {
		fprintf(stderr, "Invalid argument. Usage: ./lab1b_server --port=portno --log=filename --encrypt=filename\n");
		exit(1);
	}

	static struct option long_options[] = {
		{"port", required_argument, NULL, 'p'},
		{"log", required_argument, NULL, 'l'},
		{"encrypt", required_argument, NULL, 'e'},
		{0,0,0,0}
	};

	while(1) {
		ret = getopt_long(argc, argv, "p:l:e:", long_options, NULL);
		if (ret == -1) {
			break;
		}

		switch (ret) 
		{
			case 'p':
			{
				portno = atoi(optarg);	//get port number
				break;
			}
			case 'l':
			{
				lFlag = 1;
				logfile = optarg;	////open log file
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
				fprintf(stderr, "Invalid arguments. Usage: ./lab1b_client --port=portno --log=filename --encrypt=filename \n");
				exit(1);
			}
		}
	}

	if (lFlag) {
		if (logfile != NULL) {
			logfd = creat(logfile, 0666);
			if (logfd == -1) {
				fprintf(stderr, "log file %s\n", strerror(errno));
				exit(1);
			}
		}
	}

	if (eFlag) {
		//get data about my.key
		struct stat key_stat;
		int keyfd = 0;
		if (keyfile != NULL) {
			keyfd = open(keyfile, O_RDONLY);
			if (keyfd < 0) {
				fprintf(stderr, "open key %s\n", strerror(errno));
				exit(1);
			}
		}
		if (fstat(keyfd, &key_stat) < 0) {
			fprintf(stderr, "key stat %s\n", strerror(errno));
			exit(1);
		}
		keylen = key_stat.st_size;
		key = (char*)malloc(keylen*sizeof(char));
		if (read(keyfd, key, keylen) < 0) {
			fprintf(stderr, "read key %s\n", strerror(errno));
			exit(1);
		}

		//initialize encryption and decryption
		encr_fd = mcrypt_module_open("twofish", NULL, "cfb", NULL);
		if (encr_fd == MCRYPT_FAILED) {
			fprintf(stderr, "ERROR: Failed opening module\n");
			exit(1);
		}
		if (mcrypt_generic_init(encr_fd, key, keylen, NULL) < 0) {
			fprintf(stderr, "ERROR: Failed initializing encryption buffer\n");
			exit(1);
		}

		decr_fd = mcrypt_module_open("twofish", NULL, "cfb", NULL);
		if (decr_fd == MCRYPT_FAILED) {
			fprintf(stderr, "ERROR: Failed opening module\n");
			exit(1);
		}
		if(mcrypt_generic_init(decr_fd, key, keylen, NULL) < 0) {
			fprintf(stderr, "ERROR: Failed initializing decryption buffer\n");
			exit(1);
		}
	}

	set_input_mode();

	//create a socket point
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "create socket %s\n", strerror(errno));
		exit(1);
	}

	server = gethostbyname("127.0.0.1");

	if (server == NULL) {
		fprintf(stderr, "gethostbyname %s\n", strerror(errno));
		exit(0);
	}

	//Initialize socket, fill memory with NULL bytes
	//bzero and bcopy works too, but outdated
	memset((char*)&serv_addr, 0, sizeof(serv_addr));	
	serv_addr.sin_family = AF_INET;
	memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	//now connect to the server
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "connect %s\n", strerror(errno));
		exit(1);
	}
	
	//send input from keyboard to socket (while echoing to the display) and stdout
	//and read from socket and output to stdout

	struct pollfd fds[2];
	int timeout = 0;
	int pRet = 0;

	fds[0].fd = STDIN_FILENO;	//reading from keyboard
	fds[1].fd = sockfd;	//reading from socket
	fds[0].events = POLLIN | POLLHUP | POLLERR;
	fds[1].events = POLLIN | POLLHUP | POLLERR;

	while(1) {
		pRet = poll(fds, 2, timeout);

		if (pRet < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		else if (pRet > 0) {
			//if received input from keyboard, write to STDOUT and socket
			if (fds[0].revents & POLLIN) {
				memset(buffer, 0, 256);
				count = read(STDIN_FILENO, buffer, buff_size);

				if (count == 0) {
					exit(0);
				}

				for (i = 0; i < count; i++) {
					char c = buffer[i];

					if (c == '\r' || c == '\n') {
						write(STDOUT_FILENO, retn, 2);
						buffer[i] = '\n';
						//write(sockfd, buffer+i, 1);
					}
					else {
						write(STDOUT_FILENO, &c, 1);
						//write(sockfd, &c, 1);
					}
				}
				//write buffer to socket
				if (eFlag) {
					if (mcrypt_generic(encr_fd, buffer, count) != 0) {
						fprintf(stderr, "ERROR: encryption did not succeed.\n");
						exit(1);
					}
				}
				//write sent message to log if lFlag is on
				if (lFlag) {
					logging(buffer, count, 1);
				}
				write(sockfd, buffer, count);
			}

			//if received input from the socket, write to STDOUT
			if (fds[1].revents & POLLIN) {
				memset(buffer, 0, 256);
				count = read (sockfd, buffer, buff_size);
				if (count == 0) {
					exit(0);
				}
				else {
					//write receiving message to log if lFlag is on
					if (lFlag) {
						logging(buffer, count, 0);
					}

					//decrypt before outputing
					if (eFlag) {
						if (mdecrypt_generic(encr_fd, buffer, count) != 0) {
							fprintf(stderr, "ERROR: encryption did not succeed.\n");
							exit(1);
						}
					}

					//output buffer
					for (i = 0; i < count; i++) {
						char c = buffer[i];

						if (c == '\n') {
							write(STDOUT_FILENO, retn, 2);
						}
						else {
							write(STDOUT_FILENO, &c, 1);
						}
					}
				}
			}
		}
	}
}