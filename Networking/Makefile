CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99 -ggdb
server: server.c
	${CC} ${CFLAGS} $^ -o $@

.PHONY: clean

clean:
	rm -f server
