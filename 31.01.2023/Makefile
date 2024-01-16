CFLAGS = -Wall
LDFLAGS = -lpthread

.PHONY: all clean

all: main

main: main.o thread.o
	gcc $^ -o main $(LDFLAGS)

thread.o: thread.h thread.c
	gcc -c $(CFLAGS) thread.c -o thread.o

main.o: main.c thread.h
	gcc -c $(CFLAGS) main.c -o main.o

clean:
	-rm main *.o