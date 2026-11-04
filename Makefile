CC = gcc
CFLAGS = -Wall -g -std=gnu90

all: test

test: test.o lwp.o rr.o magic64.o
	$(CC) $(CFLAGS) -o test test.o lwp.o rr.o magic64.o

test.o: test.c lwp.h
	$(CC) $(CFLAGS) -c test.c

lwp.o: lwp.c lwp.h fp.h rr.h
	$(CC) $(CFLAGS) -c lwp.c

rr.o: rr.c rr.h lwp.h
	$(CC) $(CFLAGS) -c rr.c

magic64.o: magic64.S
	$(CC) -c magic64.S

clean:
	rm -f *.o test