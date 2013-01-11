#
# Makefile for spin compiler
# Copyright (c) 2011 Total Spectrum Software Inc.
#

#
# WARNING: byacc probably will not work!
#
#YACC = byacc
YACC = bison
CC = gcc
#CFLAGS = -g -Wall -Werror
#CC = clang
CFLAGS = -O -g -Wall -Werror
LIBS = -lm
RM = rm -f

PROGS = testlex spin2cpp
LEXOBJS = lexer.o symbol.o ast.o expr.o flexbuf.o preprocess.o
OBJS = $(LEXOBJS) spin.tab.o functions.o pasm.o outcpp.o outdat.o

all: $(PROGS)

testlex: testlex.c $(LEXOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

spin.tab.c spin.tab.h: spin.y
	$(YACC) -t -b spin -d spin.y

lexer.c: spin.tab.h

clean:
	$(RM) $(PROGS) *.o spin.tab.c spin.tab.h

test: $(PROGS)
	./testlex
	(cd Test; ./runtests.sh)

spin2cpp: spin2cpp.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
