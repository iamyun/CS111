#NAME: Yun Xu
#EMAIL: x_one_u@yahoo.com
#ID: 304635157

default: 
	gcc -pthread -o lab2_list -g lab2_list.c SortedList.c

build:
	gcc -g -lpthread -o lab2_list -L /u/eng/class/classpqf/lib/ -lprofiler lab2_list.c SortedList.c

tests:
	./lab2_list --threads=1  --iterations=1000 --sync=m >> lab2_list.csv
	./lab2_list --threads=2  --iterations=1000 --sync=m >> lab2_list.csv
	./lab2_list --threads=4  --iterations=1000 --sync=m >> lab2_list.csv
	./lab2_list --threads=8  --iterations=1000 --sync=m >> lab2_list.csv
	./lab2_list --threads=12 --iterations=1000 --sync=m >> lab2_list.csv
	./lab2_list --threads=16 --iterations=1000 --sync=m >> lab2_list.csv
	./lab2_list --threads=24 --iterations=1000 --sync=m >> lab2_list.csv
	./lab2_list --threads=1  --iterations=1000 --sync=s >> lab2_list.csv
	./lab2_list --threads=2  --iterations=1000 --sync=s >> lab2_list.csv
	./lab2_list --threads=4  --iterations=1000 --sync=s >> lab2_list.csv
	./lab2_list --threads=8  --iterations=1000 --sync=s >> lab2_list.csv
	./lab2_list --threads=12 --iterations=1000 --sync=s >> lab2_list.csv
	./lab2_list --threads=16 --iterations=1000 --sync=s >> lab2_list.csv
	./lab2_list --threads=24 --iterations=1000 --sync=s >> lab2_list.csv

profile: lab2_list Makefile
	-rm -f ./raw.gperf
	LD_PRELOAD=/u/eng/class/classpqf/lib/libprofiler.so.0 CPUPROFILE=raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	pprof --text ./lab2_list ./raw.gperf > profile.out
	pprof --list=thread_routine ./lab2_list ./raw.gperf >> profile.out
	rm -f ./raw.gperf

graphs:
	chmod +x lab2_list.gp
	gnuplot ./lab2_list.gp

dist:
	tar -zcvf lab2b-304635157.tar.gz lab2_list.c SortedList.c SortedList.h lab2_list.csv lab2_list.gp *.png Makefile README

clean:
	rm -f lab2_list *.o *.png *.csv *.tar.gz