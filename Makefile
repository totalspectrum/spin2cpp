#
# Makefile for spin compiler
# Copyright (c) 2011-2016 Total Spectrum Software Inc.
# Distributed under the MIT License (see COPYING for details)
#
# if CROSS is defined, we are building a cross compiler
# possible targets are: win32, rpi
# Note that you may have to adjust your compiler names depending on
# which Linux distribution you are using (e.g. ubuntu uses
# "i586-mingw32msvc-gcc" for mingw, whereas Debian uses
# "i686-w64-mingw32-gcc"
#

ifeq ($(CROSS),win32)
#  CC=i586-mingw32msvc-gcc
  CC=i686-w64-mingw32-gcc
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

HEADERS = $(BUILD)/spin.tab.h ast.h expr.h flexbuf.h lexer.h preprocess.h spinc.h symbol.h

PROGS = $(BUILD)/testlex$(EXT) $(BUILD)/spin2cpp$(EXT)

LEXSRCS = lexer.c symbol.c ast.c expr.c flexbuf.c preprocess.c
SPINSRCS = $(LEXSRCS) functions.c pasm.c outcpp.c outdat.c p1ir.c

LEXOBJS = $(LEXSRCS:%.c=$(BUILD)/%.o)
SPINOBJS = $(SPINSRCS:%.c=$(BUILD)/%.o)

OBJS = $(SPINOBJS) $(BUILD)/spin.tab.o

all: $(BUILD) $(PROGS)

$(BUILD)/testlex$(EXT): testlex.c $(LEXOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/spin.tab.c $(BUILD)/spin.tab.h: spin.y
	$(YACC) -t -b $(BUILD)/spin -d spin.y

clean:
	$(RM) $(PROGS) $(BUILD)/*

test: lextest cpptest errtest runtest

lextest: $(PROGS)
	$(BUILD)/testlex

cpptest: $(PROGS)
	(cd Test; ./cpptests.sh)

errtest: $(PROGS)
	(cd Test; ./errtests.sh)

runtest: $(PROGS)
	(cd Test; ./runtests.sh)

$(BUILD)/spin2cpp$(EXT): spin2cpp.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/spin.tab.o: $(BUILD)/spin.tab.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/%.o: %.c $(HEADERS)
	$(CC) -MMD -MP $(CFLAGS) -o $@ -c $<

#
# target to build a .zip file for a release
#
zip:
	$(MAKE) CROSS=win32
	$(MAKE) CROSS=linux32
	zip spin2cpp_v1.xx.zip README.md COPYING Changelog.txt spin2cpp.exe spin2cpp.linux


