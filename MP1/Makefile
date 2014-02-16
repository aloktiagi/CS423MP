obj-m = cpum.o

cpum-objs := \
	main.o \
	proc.o \
        usertime.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

all: test

test:
	gcc -o test testcode.c
