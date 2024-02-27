CC = gcc
CFLAGS = -g -std=c99 -Wall -fsanitize=address,undefined

all: ttts ttt

ttts: ttts.o 
	$(CC) $(CFLAGS) -pthread -o $@ $^

ttt: ttt.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f ttts ttt *.o