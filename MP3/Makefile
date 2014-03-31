obj-m = prof.o

prof-objs := \
	main.o \
	proc.o \
    profiler.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

all: work

test:
	gcc -o work work.c
