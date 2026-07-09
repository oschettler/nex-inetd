CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic
LDFLAGS =
TARGETS = nexd spartand
PREFIX ?= /usr/local

.PHONY: all clean install uninstall test

all: $(TARGETS)

nexd: nexd.c
	$(CC) $(CFLAGS) -o nexd nexd.c $(LDFLAGS)

spartand: spartand.c
	$(CC) $(CFLAGS) -o spartand spartand.c $(LDFLAGS)

clean:
	rm -f $(TARGETS)

install: $(TARGETS)
	install -d $(PREFIX)/bin
	install -m 755 nexd $(PREFIX)/bin/
	install -m 755 spartand $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/nexd $(PREFIX)/bin/spartand

test: $(TARGETS)
	./test.sh
