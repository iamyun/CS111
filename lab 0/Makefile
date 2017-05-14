#NAME: Yun Xu
#EMIAL: x_one_u@yahoo.com
#ID: 304635157

default:
	gcc -o lab0 -g lab0.c

check: checkSuccess checkArg checkInput checkOutput checkCatch

checkSuccess:
	echo "Good Night" > in.txt;\
	./lab0 --input=in.txt --output=out.txt;\
	if [ $$? -eq 0 ]; then echo "pass Success Check";\
	else echo "fail Success Check";\
	fi

checkArg:
	./lab0 --helloworld;\
	if [ $$? -eq 1 ]; then echo "pass Argument Check";\
	else echo "fail Argument Check";\
	fi

checkInput:
	./lab0 --input=hi.txt;\
	if [ $$? -eq 2 ]; then echo "pass Input Check";\
	else echo "fail Input Check";\
	fi

checkOutput:
	touch hello.txt;\
	chmod 444 hello.txt;\
	./lab0 --output=hello.txt;\
	if [ $$? -eq 3 ]; then echo "pass Output Check";\
	else echo "fail Output Check";\
	fi

checkCatch:
	./lab0 --segfault --catch;\
	if [ $$? -eq 4 ]; then echo "pass Catch Check";\
	else echo "fail Catch Check";\
	fi

clean:
	rm -f lab0 *.o *.txt *.tar.gz

dist:
	tar -czf lab0-304635157.tar.gz Makefile README lab0.c backtrace.png breakpoint.png