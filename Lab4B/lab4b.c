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

int period = 1;
char scale = 'F';
int lFlag = 0;
int logfd;

sig_atomic_t volatile run_flag = 1;
mraa_gpio_context btn;
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
	if (mraa_gpio_read(btn) == 1 || offFlag == 1) {
		fprintf(stdout, "%s SHUTDOWN\n", timebuf);	
		if (lFlag == 1) {
			length = sprintf(print, "%s SHUTDOWN\n", timebuf);
			write(logfd, &print, length);
		}
		exit(0);
	}
	else {
		if (timeDiff >= period && stopFlag == 0) {
			if (scale == 'F') {
				fprintf(stdout, "%s %.1f\n", timebuf, fahr);
				if (lFlag == 1) {
					length = sprintf(print, "%s %.1f\n", timebuf, fahr);
					write(logfd, &print, length);
				} 
			}
			else {
				fprintf(stdout, "%s %.1f\n", timebuf, celc);
				if (lFlag == 1) {
					length = sprintf(print, "%s %.1f\n", timebuf, celc);
					write(logfd, &print, length);
				} 
			}
			prevTime = curTime;
		}
	}
}

void do_when_interrupted(int sig) {
	if (sig = SIGINT)
		run_flag = 0;
}

int main(int argc, char **argv) {
	btn = mraa_gpio_init(3);
	tempSensor = mraa_aio_init(0);
	
	mraa_gpio_dir(btn, MRAA_GPIO_IN);

	int ret = 0;
	char *logfile = NULL;
	static struct option long_options[] = {
		{"period", required_argument, NULL, 'p'},
		{"scale", required_argument, NULL, 's'},
		{"log", required_argument, NULL, 'l'},
		{0,0,0,0}
	};

	while(run_flag) {
		ret = getopt_long(argc, argv, "p:s:l:", long_options, NULL);
		if (ret == -1) {
			break;
		}

		switch(ret) {
			case 'p':
				period = atoi(optarg);
				break;
			case 's':
				scale = optarg[0];
				break;
			case 'l':
				lFlag = 1;
				logfile = optarg;
				break;
			default:
				fprintf(stderr, "Invalid argument. Usage: ./lab4b --period=# --scale=C/F --log=filename\n");
				exit(1);
		}
	}
 
	if (lFlag) {
		if (logfile != NULL){
			logfd = creat(logfile, 0666);
			if (logfd == -1) {
				fprintf(stderr, "log file %s\n", strerror(errno));
				exit(1);
			}
		}
	}

	struct pollfd fd[1];
	int timeout = 0;
	int pRet= 0;

	fd[0].fd = STDIN_FILENO;
	fd[0].events = POLLIN | POLLHUP | POLLERR;

	while(1) {
		curTime = time(NULL);
		getTemp();
		pRet = poll(fd, 1, timeout);

		if (pRet < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		else if (pRet > 0) {
			char c;
			if (fd[0].revents & POLLIN) {
				while(1) {
					if (read(STDIN_FILENO, &c, 1) > 0) {
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
						exit(1);
					}
				}
				
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

	mraa_aio_close(tempSensor);
	mraa_gpio_close(btn);
	return 0;
}