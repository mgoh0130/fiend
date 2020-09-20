# NAME: Michelle Goh
#   NetId: mg2657
CC=gcc
CFLAGS= -std=c99 -pedantic -Wall -g3

all:	fiend

fiend:	fiend.o
		${CC} ${CFLAGS} $^ -o $@

fiend.o:	fiend.h

clean:
		rm -f fiend *.o