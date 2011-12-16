#
# Makefile for spin compiler
# Copyright (c) 2011 Total Spectrum Software Inc.
#

CC = gcc
CFLAGS = -g -Wall -Werror
YACC = bison
RM = rm -f

PROGS = test
OBJS = lexer.o spin.tab.o

all: $(PROGS)

test: testlex.c lexer.o
	$(CC) $(CFLAGS) -o $@ $^

spin.tab.c spin.tab.h: spin.y
	$(YACC) -t -b spin -d spin.y

lexer.c: spin.tab.h

clean:
	$(RM) $(PROGS) *.o