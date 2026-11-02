CC = gcc
CFLAGS = -Wall -g -std=gnu90

all: lwp.o magic64.o

lwp.o: lwp.c lwp.h fp.h
	$(CC) $(CFLAGS) -c lwp.c -o lwp.o

magic64.o: magic64.S 
	$(CC) -c magic64.S -o magic64.o

clean:
	rm -f *.o