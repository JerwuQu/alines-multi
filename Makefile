CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pthread

.PHONY: all
all: alines-menu alines-server

alines-menu: alines-menu.c
	$(CC) $(CFLAGS) -o $@ $^

alines-server: alines-server.c
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f alines-menu alines-server

.PHONY: test
test: alines-menu alines-server
	./alines-server './test.sh'

