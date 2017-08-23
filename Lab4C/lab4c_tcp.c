#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <mraa/aio.h>
#include <getopt.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

int period = 1;
char scale = 'F';
int lFlag = 0;
int logfd;
int portno = 18000;
int sockfd;
int sockFlag = 0;

sig_atomic_t volatile run_flag = 1;
mraa_aio_context tempSensor;

const int B = 4275;	//B value of the thermistor
const int R0 = 100000;	//A0 = 100K
float celc, fahr;
time_t timeStamp;
struct tm* timeStruct;
char timebuf[9]; 

int offFlag = 0;
int stopFlag = 0;

time_t prevTime = 0;
time_t curTime;
time_t timeDiff;

int offset = 0;
char command[256];

void exitNow(int exitCode) {
	if (sockFlag == 1) {
		close(sockfd);
	}

	mraa_aio_close(tempSensor);

	exit(exitCode);
}

void getTemp()
{
	timeDiff = curTime - prevTime;

	time(&timeStamp);
	timeStruct = localtime(&timeStamp);
	strftime(timebuf, 9, "%H:%M:%S", timeStruct);

	int temp;

	temp = mraa_aio_read(tempSensor);
	float R = 1023.0/((double)temp) - 1.0;
	R = R0*R;

	celc = 1.0/(log(R/R0)/B + 1/298.15) - 273.15; 
	fahr = celc*1.8+32;
	
	char print[20];
	int length = 0;
	if (offFlag == 1) {
		//fprintf(stdout, "%s SHUTDOWN\n", timebuf);	
		if (lFlag == 1) {
			length = sprintf(print, "%s SHUTDOWN\n", timebuf);
			write(logfd, &print, length);
			write(sockfd, &print, length);
		}
		exitNow(0);
	}
	else {
		if (timeDiff >= period && stopFlag == 0) {
			if (scale == 'F') {
				//fprintf(stdout, "%s %.1f\n", timebuf, fahr);
				if (lFlag == 1) {
					length = sprintf(print, "%s %.1f\n", timebuf, fahr);
					write(logfd, &print, length);
					write(sockfd, &print, length);
				} 
			}
			else {
				//fprintf(stdout, "%s %.1f\n", timebuf, celc);
				if (lFlag == 1) {
					length = sprintf(print, "%s %.1f\n", timebuf, celc);
					write(logfd, &print, length);
					write(sockfd, &print, length);
				} 
			}
			prevTime = curTime;
		}
	}
}

int main(int argc, char **argv) {
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char *id = NULL;
	char *hostName = NULL;

	tempSensor = mraa_aio_init(0);
	int argc_count = 1;

	if (argc < 4) {
		fprintf(stderr, "Invalid argument. Usage: ./lab4c_tcp --id=# --host=hostName --log=filename portNumber\n");
		exitNow(1);
	}

	int ret = 1;
	char *logfile = NULL;
	static struct option long_options[] = {
		{"id", required_argument, NULL, 'i'},
		{"host", required_argument, NULL, 'h'},
		{"log", required_argument, NULL, 'l'},
		{0,0,0,0}
	};

	while(run_flag) {
		ret = getopt_long(argc, argv, "i:h:l:", long_options, NULL);
		if (ret == -1) {
			break;
		}

		switch(ret) {
			case 'i':
				id = optarg;
				argc_count++;
				break;
			case 'h':
				hostName = optarg;
				argc_count++;
				break;
			case 'l':
				lFlag = 1;
				logfile = optarg;
				argc_count++;
				break;
			default:
				fprintf(stderr, "Invalid argument. Usage: ./lab4c_tcp --id=# --host=hostName --log=filename portNumber\n");
				exitNow(1);
		}
	}

	if (argc - argc_count == 1) {
		portno = atoi(argv[argc_count]);
	}
	else {
		fprintf(stderr, "Invalid argument. Usage: ./lab4c_tcp --id=# --host=hostName --log=filename portNumber\n");
		exitNow(1);
	}
 
	if (lFlag) {
		if (logfile != NULL){
			logfd = creat(logfile, 0666);
			if (logfd == -1) {
				fprintf(stderr, "log file %s\n", strerror(errno));
				exitNow(1);
			}
		}
	}

	//create a socket point
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "create socket %s\n", strerror(errno));
		exitNow(2);
	}

	server = gethostbyname(hostName);
	if (server == NULL) {
		fprintf(stderr, "gethostbyname %s\n", strerror(errno));
		exitNow(1);
	}

	sockFlag = 1;

	//Initialize socket, fill memory with NULL bytes
	//bzero and bcopy works too, but outdated
	memset((char*)&serv_addr, 0, sizeof(serv_addr));	
	serv_addr.sin_family = AF_INET;
	memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	//now connect to the server
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "connect %s\n", strerror(errno));
		exitNow(2);
	}

	struct pollfd fd[1];
	int timeout = 0;
	int pRet= 0;

	char id_buff[15];
	int len = sprintf(id_buff, "ID=%s\n", id);
	write(sockfd, &id_buff, len);

	fd[0].fd = sockfd;	//reading from keyboard
	fd[0].events = POLLIN | POLLHUP | POLLERR;

	while(1) {
		curTime = time(NULL);
		getTemp();
		pRet = poll(fd, 1, timeout);

		if (pRet < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exitNow(2);
		}
		else if (pRet > 0) {
			char c;
			if (fd[0].revents & POLLIN) {
				while(1) {
					if (read(sockfd, &c, 1) > 0) {
						command[offset] = c;
						offset++;
						if (c == '\n') {
							command[offset] = '\0';
							offset = 0;
							break;
						}
					}
					else {
						fprintf(stderr, "read %s\n", strerror(errno));
						exitNow(2);
					}
				}
				
				//fprintf(stderr, "%s", command);

				if (strcmp(command, "OFF\n") == 0) {
					offFlag = 1;
				}
				else if (strcmp(command, "STOP\n") == 0) {
					stopFlag = 1;
				}
				else if (strcmp(command, "START\n") == 0) {
					stopFlag = 0;
				}
				else if (strcmp(command, "SCALE=F\n") == 0) {
					scale = 'F';
				}
				else if (strcmp(command, "SCALE=C\n") == 0) {
					scale = 'C';
				}
				else if (strncmp(command, "PERIOD=", 7) == 0) {
					int i = atoi(command+7);
					if (i > 0)
						period = i;	
				}

				if (lFlag == 1) {
					int len = strlen(command);
					write(logfd, &command, len);
				}
			}
		}
	}

	
	exitNow(0);
}