CC=gcc
CFLAGS=-std=c99 -O2 -Wall -Wextra -pthread

.PHONY: all
all: alines-menu alines-server

alines-menu: alines-menu.c shared.c
	$(CC) $(CFLAGS) -o $@ $<

alines-server: alines-server.c shared.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f alines-menu alines-server

.PHONY: test
test: alines-menu alines-server
	./alines-server './test.sh'


