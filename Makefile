CC = gcc
CFLAGS = -g -Wall -Werror

all: test

test: test.c lexer.c
	$(CC) $(CFLAGS) -o $@ $^
