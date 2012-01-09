#
# Makefile for spin compiler
# Copyright (c) 2011 Total Spectrum Software Inc.
#

CC = gcc
CFLAGS = -g -Wall -Werror
#CFLAGS = -O -g -Wall -Werror
YACC = bison
RM = rm -f

PROGS = testlex spin2cpp
LEXOBJS = lexer.o symbol.o ast.o expr.o
OBJS = $(LEXOBJS) spin.tab.o functions.o pasm.o

all: $(PROGS)

testlex: testlex.c $(LEXOBJS)
	$(CC) $(CFLAGS) -o $@ $^

spin.tab.c spin.tab.h: spin.y
	$(YACC) -t -b spin -d spin.y

lexer.c: spin.tab.h

clean:
	$(RM) $(PROGS) *.o spin.tab.c spin.tab.h

test: $(PROGS)
	./testlex
	(cd Test; ./runtests.sh)

spin2cpp: spin2cpp.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
