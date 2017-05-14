#NAME: Yun Xu
#EMAIL: x_one_u@yahoo.com
#ID: 304635157

default:
	gcc -o lab1b-client -g lab1b-client.c -lmcrypt
	gcc -o lab1b-server -g lab1b-server.c -lmcrypt

client:
	gcc -o lab1b-client -g lab1b-client.c -lmcrypt

server:
	gcc -o lab1b-server -g lab1b-server.c -lmcrypt

clean:
	rm -f lab1b-server lab1b-client *.o *.txt *.tar.gz

dist: clean
	tar -zcvf lab1b-304635157.tar.gz lab1b-client.c lab1b-server.c my.key Makefile README