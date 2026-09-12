CC = gcc
CFLAGS = -Wall -Werror -Wextra
LDFLAGS = -pthread


all: switch client

switch.o: switch.c common.h
	$(CC) $(CFLAGS) -c switch.c -o switch.o

switch: switch.o
	$(CC) $(LDFLAGS) switch.o -o switch

client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c -o client.o

client: client.o
	$(CC) $(LDFLAGS) client.o -o client

clean:
	-rm -f *.o switch client

.PHONY: all clean