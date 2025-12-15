CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic
LDFLAGS =
TARGET = nexd
PREFIX ?= /usr/local

.PHONY: all clean install uninstall test

all: $(TARGET)

$(TARGET): nexd.c
	$(CC) $(CFLAGS) -o $(TARGET) nexd.c $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

test: $(TARGET)
	./test.sh
