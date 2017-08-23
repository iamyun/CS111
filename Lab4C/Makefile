#NAME: Yun Xu
#EMAIL: x_one_u@yahoo.com
#ID: 304635157

default:
	gcc -lmraa -lm -o lab4c_tcp -g lab4c_tcp.c
	gcc -lmraa -lm -lssl -lcrypto -o lab4c_tls -g lab4c_tls.c

clean: 
	rm -f lab4c_tls lab4c_tcp *.tar.gz 

dist:
	tar -zcvf lab4c-304635157.tar.gz lab4c_tls.c lab4c_tcp.c README Makefile