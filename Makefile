#
# Makefile for spin compiler
# Copyright (c) 2011-2024 Total Spectrum Software Inc. and contributors
# Distributed under the MIT License (see COPYING for details)
#
# if CROSS is defined, we are building a cross compiler
# possible targets are: win32, rpi, macosx
# Note that you may have to adjust your compiler names depending on
# which Linux distribution you are using (e.g. ubuntu uses
# "i586-mingw32msvc-gcc" for mingw, whereas Debian uses
# "i686-w64-mingw32-gcc"
#
# to use a different version of YACC from the default (bison3) define
# YACCVER to bison2 or byacc
#
# to build a release .zip, do "make zip"
# to build a signed release, do "make zip SIGN=sign.digikey.sh"
#

default: all

#
# We try to figure out the version of yacc/bison
# you are using, but you can define this explicitly
# if you want too.
#
#YACCVER=byacc
#YACCVER=bison2
#YACCVER=bison3

# use bison unless user forces something different
#
# note: to produce detailed debug, use YACC="bison --verbose --report=all" with bison 3.7.2 or later (just --verbose otherwise)
#

YACC = bison

ifndef YACCVER
YACC_CHECK := $(shell $(YACC) --version | fgrep 3.)
ifeq ($(YACC_CHECK),)
    ifeq ($(YACC),byacc)
	YACCVER=byacc
    else
	YACCVER=bison2
    endif
else
    YACCVER=bison3
endif
endif

# the script used to turn foo.exe into foo.signed.exe
SIGN ?= ./sign.dummy.sh

ifeq ($(CROSS),win32)
#  CC=i586-mingw32msvc-gcc
  CC=i686-w64-mingw32-gcc -Wl,--stack -Wl,8000000 -O
  EXT=.exe
  BUILD=build-win32
else ifeq ($(CROSS),rpi)
  CC=arm-linux-gnueabihf-gcc -O
  EXT=
  BUILD=build-rpi
else ifeq ($(CROSS),linux32)
  CC=gcc -m32
  EXT=
  BUILD=build-linux32
else ifeq ($(CROSS),macosx)
  CC=o64-clang -DMACOSX -O
  EXT=
  BUILD=build-macosx
else ifeq ($(OS),Windows_NT)
  CC=gcc
  EXT=.exe
  BUILD=build
else ifeq ($(CROSS),linux-musl)
  CC=musl-gcc -static -fno-pie
  EXT=
  BUILD=build
else
  CC=gcc
  EXT=
  BUILD=build
endif

INC=-I. -I./backends -I./frontends -I$(BUILD)
DEFS=-DFLEXSPIN_BUILD

ifeq ($(YACCVER),bison3)

RUNYACC = $(YACC) -Wno-deprecated -D parse.error=verbose
YY_SPINPREFIX= -D api.prefix={spinyy}
YY_BASICPREFIX= -D api.prefix={basicyy}
YY_CPREFIX= -D api.prefix={cgramyy}

else
YY_SPINPREFIX= -p spinyy
YY_BASICPREFIX= -p basicyy
YY_CPREFIX= -p cgramyy

ifeq ($(YACCVER),byacc)
RUNYACC = byacc -s
else
YACC ?= bison
RUNYACC = $(YACC)
endif

endif

# crufty debug code
#check:
#	echo YACC="$(RUNYACC)" YACCVER="$(YACCVER)" YACC_CHECK="$(YACC_CHECK)"

# use make OPT=-g to compile for debug, OPT=-O1 for release
OPT ?= -g

# DEPS are flags to generate dependencies; not all compilers support these
DEPS ?= -MMD -MP

#CFLAGS = $(OPT) -Wall -fwrapv $(INC) $(DEFS)
#CFLAGS = -Og -g -Wall -fwrapv -Werror -std=c99 -D_XOPEN_SOURCE=600 $(INC) $(DEFS)
#CFLAGS = -no-pie -pg -Wall -fwrapv $(INC) $(DEFS)
CFLAGS = $(OPT) -Wall -fwrapv -Wc++-compat $(INC) $(DEFS)
LIBS = -lm
RM = rm -rf

VPATH=.:util:frontends:frontends/basic:frontends/spin:frontends/c:backends:backends/asm:backends/cpp:backends/bytecode:backends/dat:backends/nucode:backends/objfile:backends/zip:backends/compress:backends/compress/lz4:mcpp

LEXHEADERS = $(BUILD)/spin.tab.h $(BUILD)/basic.tab.h $(BUILD)/cgram.tab.h ast.h frontends/common.h

PROGS = $(BUILD)/testlex$(EXT) $(BUILD)/spin2cpp$(EXT) $(BUILD)/flexspin$(EXT) $(BUILD)/flexcc$(EXT)

UTIL = dofmt.c flexbuf.c lltoa_prec.c strupr.c strrev.c strdupcat.c to_utf8.c from_utf8.c sha256.c

MCPP = directive.c expand.c mbchar.c mcpp_eval.c mcpp_main.c mcpp_system.c mcpp_support.c

LEXSRCS = lexer.c uni2sjis.c symbol.c ast.c expr.c $(UTIL) preprocess.c
PASMBACK = outasm.c assemble_ir.c optimize_ir.c asm_peep.c inlineasm.c compress_ir.c
BCBACK = outbc.c bcbuffers.c bcir.c bc_spin1.c
NUBACK = outnu.c nuir.c nupeep.c
CPPBACK = outcpp.c cppfunc.c outgas.c cppexpr.c cppbuiltin.c
COMPBACK = compress.c lz4.c lz4hc.c
ZIPBACK = outzip.c zip.c
SPINSRCS = common.c case.c spinc.c $(LEXSRCS) functions.c cse.c loops.c hloptimize.c hltransform.c types.c pasm.c outdat.c outlst.c outobj.c spinlang.c basiclang.c clang.c $(PASMBACK) $(BCBACK) $(NUBACK) $(CPPBACK) $(COMPBACK) $(ZIPBACK) $(MCPP) version.c becommon.c brkdebug.c printdebug.c

LEXOBJS = $(LEXSRCS:%.c=$(BUILD)/%.o)
SPINOBJS = $(SPINSRCS:%.c=$(BUILD)/%.o)
OBJS = $(SPINOBJS) $(BUILD)/spin.tab.o $(BUILD)/basic.tab.o $(BUILD)/cgram.tab.o

SPIN_CODE = sys/p1_code.spin.h sys/p2_code.spin.h sys/bytecode_rom.spin.h sys/nucode_util.spin.h \
    sys/common.spin.h sys/common_pasm.spin.h sys/float.spin.h sys/gcalloc.spin.h sys/gc_bytecode.spin.h sys/gc_pasm.spin.h \
    sys/nuinterp.spin.h sys/p2_brkdebug.spin.h sys/p2_lz4stub.spin.h

PASM_SUPPORT_CODE = sys/lmm_orig.spin.h sys/lmm_slow.spin.h sys/lmm_trace.spin.h sys/lmm_cache.spin.h sys/lmm_compress.spin.h

help:
	@echo "make all: builds the defaults"
	@echo "make zip: builds a .zip file for distribution"
	@echo
	@echo "there are some modifiers, e.g."
	@echo "   make all YACCVER=<yacc> CROSS=<platform> SIGN=<signing script>"
	@echo "<yacc> is one of: "
	@echo "  bison2, bison3, byacc"
	@echo "<platform> is one of: "
	@echo "  win32, rpi, linux32, macosx"
	@echo "<signing script> is a script for signing executables"


all: $(BUILD) $(PROGS)

$(BUILD)/testlex$(EXT): testlex.c $(LEXOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/spin.tab.c $(BUILD)/spin.tab.h: frontends/spin/spin.y
	$(RUNYACC) $(YY_SPINPREFIX) -t -b $(BUILD)/spin -d frontends/spin/spin.y

$(BUILD)/basic.tab.c $(BUILD)/basic.tab.h: frontends/basic/basic.y
	$(RUNYACC) $(YY_BASICPREFIX) -t -b $(BUILD)/basic -d frontends/basic/basic.y

$(BUILD)/cgram.tab.c $(BUILD)/cgram.tab.h: frontends/c/cgram.y
	$(RUNYACC) $(YY_CPREFIX) -t -b $(BUILD)/cgram -d frontends/c/cgram.y

$(BUILD)/spinc.o: spinc.c $(SPIN_CODE)
$(BUILD)/outasm.o: outasm.c $(PASM_SUPPORT_CODE)

preproc: preprocess.c $(UTIL)
	$(CC) $(CFLAGS) -DTESTPP -o $@ $^ $(LIBS)

clean:
	$(RM) $(PROGS) $(BUILD)/* *.zip

test_offline: lextest asmtest bctest cpptest errtest p2test
test: test_offline runtest
#test: lextest asmtest cpptest errtest runtest
lextest: $(PROGS)
	$(BUILD)/testlex

asmtest: $(PROGS)
	(cd Test; ./asmtests.sh)

bctest: $(PROGS)
	(cd Test; ./bctests.sh)

cpptest: $(PROGS)
	(cd Test; ./cpptests.sh)

errtest: $(PROGS)
	(cd Test; ./errtests.sh)

p2test: $(PROGS)
	(cd Test; ./p2bin.sh)

runtest: $(PROGS)
	(cd Test; ./runtests_p2.sh)

test_spinsim:  $(PROGS)
	(cd Test/spinsim; make) 
	(cd Test; ./runtests_p1.sh "" "./spinsim/build/spinsim -b -q")
	(cd Test; ./runtests_bc.sh "" "./spinsim/build/spinsim -b -q")

$(BUILD)/spin2cpp$(EXT): spin2cpp.c cmdline.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/flexspin$(EXT): flexspin.c cmdline.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/flexcc$(EXT): flexcc.c cmdline.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/spin.tab.o: $(BUILD)/spin.tab.c $(LEXHEADERS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/basic.tab.o: $(BUILD)/basic.tab.c $(LEXHEADERS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/cgram.tab.o: $(BUILD)/cgram.tab.c $(LEXHEADERS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/lexer.o: frontends/lexer.c $(LEXHEADERS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/version.o: version.c version.h FORCE
	$(eval gitbranch=$(shell git rev-parse --abbrev-ref HEAD))
	$(CC) $(CFLAGS) -DGITREV=$(shell git describe --tags --always) $(if $(filter release/%,$(patsubst master,release/master,$(gitbranch))),,-DGITBRANCH=$(gitbranch)) -o $@ -c $<

$(BUILD)/%.o: %.c
	$(CC) $(DEPS) $(CFLAGS) -o $@ -c $<

.PHONY: FORCE

#
# convert a .spin file to a header file
# note that xxd does not 0 terminate its generated string,
# which is what the sed script will do
#
sys/%.spin.h: sys/%.spin
	xxd -i $< $@
sys/%.bas.h: sys/%.bas
	xxd -i $< $@

#
# automatic dependencies
# these .d files are generated by the -MMD -MP flags to $(CC) above
# and give us the dependencies
# the "-" sign in front of include says not to give any error or warning
# if a file is not found
#
-include $(SPINOBJS:.o=.d)


COMMONDOCS=COPYING Changelog.txt doc
ALLDOCS=README.md Flexspin.md $(COMMONDOCS)

zip: all

ifeq ($(CROSS),win32)
	$(SIGN) $(BUILD)/flexspin
	mv $(BUILD)/flexspin.signed.exe $(BUILD)/flexspin.exe
	$(SIGN) $(BUILD)/flexcc
	mv $(BUILD)/flexcc.signed.exe $(BUILD)/flexcc.exe
endif
	zip -r flexptools.zip $(BUILD)/spin2cpp$(EXT) $(BUILD)/flexcc$(EXT) $(BUILD)/flexspin$(EXT) $(ALLDOCS) include
# I could not make this work in one command idk
	printf "@ $(BUILD)/spin2cpp$(EXT)\n@=bin/spin2cpp$(EXT)\n" | zipnote -w flexptools.zip
	printf "@ $(BUILD)/flexcc$(EXT)\n@=bin/flexcc$(EXT)\n" | zipnote -w flexptools.zip
	printf "@ $(BUILD)/flexspin$(EXT)\n@=bin/flexspin$(EXT)\n" | zipnote -w flexptools.zip

# target to build preprocessor
preprocess: preprocess.c util/flexbuf.c util/dofmt.c util/strupr.c util/strrev.c util/lltoa_prec.c
	$(CC) -o $@ -g -DTESTPP $^ -lm

#
# target to build a windows spincvt GUI
# this has almost certainly bit-rotted
#
#FREEWRAP=/opt/freewrap/linux64/freewrap
#FREEWRAPEXE=/opt/freewrap/win32/freewrap.exe
#
#spincvt.zip: .PHONY
#	rm -f spincvt.zip
#	rm -rf spincvt
#	$(MAKE) CROSS=win32
#	mkdir -p spincvt/bin
#	cp build-win32/spin2cpp.exe spincvt/bin
#	cp propeller-load/propeller-load.exe spincvt/bin
#	cp spinconvert/spinconvert.tcl spincvt
#	mkdir -p spincvt/examples
#	cp -rp spinconvert/examples/*.spin spincvt/examples
#	cp -rp spinconvert/examples/*.def spincvt/examples
#	cp -rp spinconvert/README.txt COPYING spincvt
#	cp -rp doc spincvt
#	(cd spincvt; $(FREEWRAP) spinconvert.tcl -w $(FREEWRAPEXE))
#	rm spincvt/spinconvert.tcl
#	zip -r spincvt.zip spincvt
#	rm -rf spincvt


.PHONY:
