#
# Makefile for spin compiler
# Copyright (c) 2011-2015 Total Spectrum Software Inc.
# Distributed under the MIT License (see COPYING for details)
#
# if CROSS is defined, we are building a cross compiler
# possible targets are: win32, rpi

ifeq ($(CROSS),win32)
  CC=i586-mingw32msvc-gcc
  EXT=.exe
  BUILD=./build-win32
else ifeq ($(CROSS),rpi)
  CC=arm-linux-gnueabihf-gcc
  EXT=
  BUILD=./build-rpi
else ifeq ($(CROSS),linux32)
  CC=gcc -m32
  EXT=
  BUILD=./build-linux32
else
  CC=gcc
  EXT=
  BUILD=./build
endif

INC=-I. -I$(BUILD)

#
# WARNING: byacc probably will not work!
#
#YACC = byacc
YACC = bison
CFLAGS = -g -Wall -Werror $(INC)
#CFLAGS = -O -g -Wall -Werror $(INC)
LIBS = -lm
RM = rm -f

PROGS = $(BUILD)/testlex$(EXT) $(BUILD)/spin2cpp$(EXT)
LEXOBJS = $(BUILD)/lexer.o $(BUILD)/symbol.o $(BUILD)/ast.o \
	$(BUILD)/expr.o $(BUILD)/flexbuf.o $(BUILD)/preprocess.o

OBJS = $(LEXOBJS) $(BUILD)/spin.tab.o $(BUILD)/functions.o \
	$(BUILD)/pasm.o $(BUILD)/outcpp.o $(BUILD)/outdat.o

all: $(BUILD) $(PROGS)

$(BUILD)/testlex$(EXT): testlex.c $(LEXOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/spin.tab.c $(BUILD)/spin.tab.h: spin.y
	$(YACC) -t -b $(BUILD)/spin -d spin.y

lexer.c: $(BUILD)/spin.tab.h

clean:
	$(RM) $(PROGS) $(BUILD)/*.o $(BUILD)/spin.tab.c $(BUILD)/spin.tab.h

test: $(PROGS)
	$(BUILD)/testlex
	(cd Test; ./runtests.sh)

$(BUILD)/spin2cpp$(EXT): spin2cpp.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/spin.tab.o: $(BUILD)/spin.tab.c
	$(CC) $(CFLAGS) -o $@ -c $^

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^
