#NAME: Yun Xu
#EMAIL: x_one_u@yahoo.com
#ID: 304635157

default:
	gcc -lmraa -lm -g -o lab4b lab4b.c

check:
	chmod +x check.sh
	./check.sh

clean:
	rm -f lab4b *.tar.gz *.txt

dist:
	tar -zcvf lab4b-304635157.tar.gz lab4b.c Makefile check.sh README
