OVERVIEW
========

`spin2cpp` and `flexspin` are tools for converting Spin to C++ and for compiling Spin to native Propeller binaries, respectively. They can also accept a FreeBASIC-like dialect and C code as input, so the names are no longer really appropriate.

Background
----------

Spin is a language that is the "native" language of the Parallax
Propeller chip (a very cool multi-core microprocessor).  Spin is
normally compiled to a bytecode which is interpreted by a program in
ROM. This results in space efficient but slow code.  In contrast, C
compilers can produce direct machine language.

spin2cpp started off as a program to convert Spin language programs to C++.
It has grown considerably past that now, and can perform conversions like:

  * Converting Spin code directly to PASM
  * Converting BASIC to PASM (see basic.md for a description of the BASIC
    dialect used)
  * Compiling a Spin or BASIC program to executable binary (using PASM
    instructions, so *much* faster than ordinary Spin bytecodes, but larger)
  * Converting Spin or BASIC to C++ or to plain C
  * Extracting the binary portion of a DAT section
  * Converting PASM style assembly in a DAT section to a GAS style .s file
  * Compiling Spin programs on a PC or other platform (with some programmer help)
  
spin2cpp should be able to deal with any Spin program; please report
any that it cannot convert.

flexspin
--------

There is also an alternate front end `flexspin` which mimics the command
line of the `openspin` compiler, but which produces LMM binaries instead
of spin bytecode binaries. `flexspin` compiled programs will typically be
about 4-10 times faster than `openspin` ones, but will be 2-3 times as large.

Languages
---------

As mentioned, besides Spin the tools can now accept BASIC and C code as input, and the languages may be mixed in the same program. See the `docs/` folder for more detail

INSTALLATION
============

Windows
-------

To install in Windows, just unzip the release ZIP file. If you want to
use the command line version of spin2cpp, you should copy spin2cpp.exe
to wherever your propeller-elf-gcc.exe file is installed.  (In fact
spin2cpp.exe doesn't usually care where it is located, but putting it
with propeller-elf-gcc is convenient.)

If you only want to use the Openspin compatible flexspin frontend, you
only need flexspin.exe, and can ignore the other executables. In this
case you do not need a C compiler.

Similarly, if you only want to use the `--asm` option of spin2cpp to
generate PASM, then you do not need propeller-elf-gcc or anything else
from PropGCC.

Linux
-----

For Linux builds you will have to build from source. Just do `make` in this directory. The binaries are placed in `build`. To install in `$INSTALLDIR` do:
```
mkdir -p $INSTALLDIR/bin
cp build/* $INSTALLDIR/bin
cp -r include $INSTALLDIR
```

COMMAND LINE USAGE
==================

flexspin
--------

flexspin is a simple interface to the SPIN -> PASM converter. It acts very
much like openspin, including mimicking its console output, so that it
can easily be used in IDEs in place of openspin. The basic usage is
```
    flexspin file.spin
```
which will produce `file.binary`, an executable which may be downloaded
to the Propeller. There are various options, use `flexspin -h` to see them
all. Probably the most significant is `-2` to compile a Propeller 2
executable:
```
   flexspin -2 file.spin
```
produces a `file.binary` which can be run on a Prop2 FPGA.

As an alternative, flexspin can also mimic the bstc command line
compiler, if it is named something that starts with the letters "bstc"
(e.g. "bstc.flexspin").

spin2cpp
--------

spin2cpp is the general command line tool. To use it, just give the name of the
.spin file it should convert, e.g:

    spin2cpp test.spin

This will produce a header file test.h and some C++ code test.cpp. It
will also automatically translate any .spin files included as objects,
and produce .cpp and .h files for them as well.

If this is a top level spin object and you plan to compile it, you may
want spin2cpp to automatically compile the object and all
dependencies:

    spin2cpp --elf -O test.spin

This will create a test.elf file that is ready to run with
propeller-load. You can also pass propgcc command line arguments
through to the C++ compiler, as long as you place them after
the --elf argument; for example:

    spin2cpp --elf -o my.elf -Os test.spin

creates the output file "my.elf" instead of "test.elf", and uses
optimization level -Os. (It is strongly recommended to pass some
form of optimization to gcc).

You can output a .binary file (like bstc and openspin do with the -b option)
instead of .elf. For example, to convert to C code and then compile that in
CMM mode, you can do:

    spin2cpp --binary -mcmm -Os test.spin

This will create "test.binary", ready to be run with propeller-load or with
any other Spin loader. The `-mcmm` and `-Os` options are passed to PropGCC;
you could also use `-mlmm` to produce LMM code.

If you just want to convert a top level object to C++ (or C), you may
want spin2cpp to automatically insert a main() function and a call to
the first method of the object. To do this, give spin2cpp the --main
option. `--main` is implied by `--elf` or `--binary`, so you do not
have to explicitly give it in those cases.


Examples
--------

The examples below use spin2cpp in a CLI and assume that the
appropriate C compilers are in your PATH.

(1) To compile the Count.spin demo with propeller-elf-gcc in
C++ mode, do:

    spin2cpp --elf -O -o Count.elf Count.spin

This produces an executable file Count.elf which may be loaded with
propeller-load.

(2) To compile the Count.spin demo with Catalina, do:

    spin2cpp --main --ccode --files Count.spin

spin2cpp will print:
    Count.c
    FullDuplexSerial.c

showing you the files that it produced. Now you can run catalina:

    catalina Count.c FullDuplexSerial.c -lc -C NO_HMI

This produces Count.binary, which may be downloaded and run as usual.


(3) To just convert a .spin file into a .c file:

    spin2cpp --ccode F32.spin

This produces .c and .h files which can be compiled together
with your other C code.

(4) To convert the PASM portion of a .spin file into a GAS .S file:

    spin2cpp --dat --gas FullDuplexSerial.spin

This produces a file FullDuplexSerial.S which contains the GAS syntax
translation of the PASM portion of FullDuplexSerial.spin. Beware that
--gas support is still experimental, and the output may need some
manual tweaking to make it correct.

See Demo/Makefile for more examples.

(5) spin2cpp also includes a simple compiler, so it can produce PASM and
binary output without PropGCC. To use this, use the --asm switch. For example,
to produce a binary with code in HUB (LMM mode) do:

    spin2cpp --asm --binary --code=hub demo.spin

This will produce demo.pasm (the converted assembly code) and demo.binary (the
compiled binary suitable for download to the device).

The alternate frontend `flexspin` is a more direct interface to the PASM compiler.

OPTIONS
=======

Spin2cpp accepts the following options:

`--asm`
  Produce (somewhat) readable PASM code as output. This bypasses PropGCC
  altogether. The result may be fed back into spin2cpp and compiled to
  a binary by adding the --binary flag after --asm, or by running
  `spin2cpp --dat --binary` on the generated .pasm file.
  
`--binary`
  Run the compiler and output a loadable binary file. Note that
  this option imples --main. Also note that after --binary you may
  specify options to be passed to PropGCC, such as -Os or -mcmm.
  If --binary appears after --dat or --asm, then the binary is produced
  by spin2cpp itself directly, and PropGCC is not invoked. Note that the
  order of the options matter: --binary must come after --asm.
  
`--cc=<compiler>`
  Use `<compiler>` as the C/C++ compiler instead of `propeller-elf-gcc`.
  This also causes some Propeller specific contents to be protected by
  `#ifdef __propeller__`, so the output may be more suitable for
  compilation on other platforms if Propeller specific registers or
  functions are avoided. If used in conjunction with some code
  annotations and ifdefs it may be possible to run simple Spin programs
  on a PC or Arduino.
  The `--cc=` option must come before any other options that set the output
  type (e.g. `--elf`).
  
`--ccode`
  Output C code instead of C++. Note that in C mode the translated methods
  have a first parameter "self" which points to the object's data.
  This is similar to the way the C++ compiler implements object methods
  internally, but in C it has to be exposed explicitly. This extra parameter
  is added automatically by spin2cpp.

`--cse`
  Enable common subexpression elimination. This optimization is the default
  for ASM output. It's not usually useful for C output, since most C
  compilers can already do this.

`--ctypes`
  Attempt to infer C types for parameters, object variables, and functions,
  based on usage within the code. This doesn't always work, and as a result
  may cause some compiler warnings. The GCC `-fpermissive` flag may be
  necessary in C++ mode.
  
`--code=cog`
`--code=hub`
  Only for PASM output (--asm option); specify whether code is to be
  placed in COG or HUB memory. The default
  is to use COG memory for code.
  
`--data=cog`
`--data=hub`
   Only for PASM output (--asm option); specify whether data is to be
   placed in COG or HUB memory. The default is to use HUB memory for
   data. Note that placing data in COG memory is experimental and
   probably won't work properly for byte or word data.


`--dat`
  Output a binary blob of the DAT section only, similar to the bstc -c
  option; or, if --gas is given, output GAS assembly for the DAT
  section. If --binary is given after --dat, prepends an appropriate
  Spin executable header so the resulting output is executable.

`--eeprom`
  Like `--binary`, but pads the file out to fill a 32768 byte EEPROM.
  
`--elf`

  After converting to C or C++, run PropGCC on the result and output
  a linked executable ELF file. Note that this option imples --main.
  Also note that after --elf you may specify options to be passed to
  PropGCC, such as -Os or -mxmmc.

`--files`
  Print a list of the .cpp (or .c) files that were produced by
  spin2cpp. Useful for tracking object dependencies.

`--fixed`
  Use 16.16 fixed point in place of IEEE floating point. In Spin this
  just affects the data format used for floats, but in BASIC this also
  changes the internal runtime routines. Fixed point is much faster, but
  has a fixed, smaller precision.

`--fcache=N`
  Sets the size of the FCACHE area, in instructions. On the P1 when
  code is placed in HUB, some small loops are compiled to be loaded
  into a region of COG memory (the FCACHE) to improve performance. The
  default FCACHE size is 64. Larger sizes may improve performance, but
  at the risk of running out of COG memory space. The minimum size is
  8; any size less than this disables FCACHE completely. This option only
  affects PASM or binary output, and is ignored for C/C++ output.
  

`--gas`
  Output inline GAS assembly code instead of binary constants. If
  given with the --dat option, produces a .S file containing the
  translation of the PASM code in the file. In other cases, causes
  the DAT section to be compiled into a separate GCC section
  containing inline GAS code. This option is still experimental and
  may not always work correctly, but it dramatically improves the
  readability and maintainability of the generated code. Without it,
  the DAT section is just an opaque binary blob; with it, the DAT
  section is readable and changeable PASM code placed inline in the C
  output.

`--main`
  Automatically add a C or C++ main() function that will invoke the default
  Spin method. Use this on top level objects only. 

`--nocse`
  Disable common subexpression elimination. This will turn off just
  the common subexpression optimization for ASM output. CSE is
  probably the riskiest of the optimizations performed by spin2cpp, so
  this flag may help if you find a bug in the compiler.

`--nopre`
  Skip the preprocessor. Normally spin2cpp runs a very simple
  preprocessor on the input.  The pre-processor understands
  #define (of simple macros, no parameters), #undef, #ifdef, #ifndef,
  #else, #elseifdef, #elseifndef, #endif, #include, #error,
  and #warning. Use of the preprocessor should not normally
  cause any issues, but this option allows you to disable it if necessary.

`--nofcache`
  Disable FCACHE. On the P1 when code is placed in HUB, some small loops
  are compiled to be loaded into a region of COG memory (the FCACHE)
  to improve performance. This option disables that optimization. It is equivalent
  to `--fcache=0`.
  
`--normalize`
  Normalize all identifiers so that the first letter is upper case and
  the rest are lower case. This is the way older versions of spin2cpp
  handled identifiers, and is useful for avoiding some identifier
  conflicts. Without this flag, identifiers are converted to all lower
  case.

`--p2`
  Create output for Propeller 2.
  
`--require=M.N.K`
  Require spin2cpp version M.N.K or later. If an earlier version number is
  detected, an error is thrown.
  
`--side`
  Create a SimpleIDE project file listing the C and C++ files created. This
  also implies `--main` to create a main entry point in the top
  level file.
  
`-g`
  Include debug information. For C and C++, this is in the form of `#line`
  directives which instruct the compiler to put references to the original
  Spin source, so gdb and similar tools will use the .spin file for debugging.
  For PASM, this just includes the original Spin source as comments in the
  PASM output.
  
`-Dname=val`
  Define a symbol for the preprocessor.

`-I path`
`-L path`
  Define a path where .spin objects will be searched for. It's OK to use
  this option multiple times.

`--version`
  Print the current spin2cpp version number, and then exit.
  
EXTENSIONS
==========

spin2cpp (and flexspin, which is just a different interface to spin2cpp)
supports a number of extensions to the Spin language. These are documented
in the file "spin.md".

spin2cpp and flexspin support compiling BASIC files. The BASIC language
is based on QuickBasic/FreeBasic, and is documented in the file "basic.md".

LIMITATIONS
===========

There are very few Spin features that are not supported yet. `_FREE` and
`_STACK` are recognized, but do nothing.

There may be other features that do not work; if you find any,
please report them so they can be fixed.

The lexer and parser are different from the Parallax ones, so they may
well report errors on code the Parallax compiler accepts.

Timing of produced code is different, of course (in general much faster than
the native Spin interpreter). This may affect some objects; sometimes
developers left out delay loops in time critical code because the Spin
interpreter is so slow they weren't necessary. Watch out for this when porting
I2C, SPI and similar functions.

Quality of Produced C and C++Code
---------------------------------

The C/C++ code produced by spin2cpp is primarily intended to be correct, and
only secondarily to be easy to read. spin2cpp does preserve the original
comments (in most cases) and tries to produce code with a similar structure
to the original Spin, but its output is not as clean as would be produced
by a human programmer. Some of the things to watch out for:

(1) Without `--gas`, DAT sections are turned into binary blobs
    (which are not maintainable). --gas helps this a lot.

(2) By default, code only uses one type (`int32_t`) cast to pointer as
    necessary; idiomatic C would use pointer types from the start. The
    `--ctypes` flag to spin2cpp tries to guess at types, and so
    improves the code quality somewhat, but still needs some work.

(3) Local variable names get lost when spin2cpp decides it has to
    force the variables into a local array (for example if the address
    of a local variable is taken)

There are probably many more.

Quality of Produced PASM Code
-----------------------------

When the `--asm` switch is used, the compiler is more concerned with
producing fast, compact code than readable code. So the output may not
be very user friendly. For example, local variables are often renamed
to things like `arg1` or `_var_01`. The compiler may also inline or
eliminate functions entirely if it thinks the resulting code will be
smaller.

Catalina Issues
---------------
The C code support in spin2cpp is still in very early stages, and so
there are some spin features which are not supported in Catalina (they
do work in PropGCC because the latter supports some C++ extensions
even in C mode).

(1) The LOOKUP and LOOKDOWN functions in Spin do not work in Catalina
unless all the arguments are constant.

(2) The reverse operator will not work in Catalina.

(3) Code produced with --gas will not work with Catalina.

C INTEROPERATION
===============

Variable Names
--------------
Spin is a case-insensitive language, which means that the strings
"str", "Str", "STR", and "sTr" all refer to the same variable. C++, on
the other hand, is case sensitive; all of those strings would be
different variables in C++.

Normally spin2cpp will convert identifiers to lower case, unless
that conflicts with a built-in C keyword or function; in that case
it will change it so that the first letter is upper case and subsequent
letters are lower case.

If the --normalize (or -n) flag is given, then spin2cpp
normalizes all Spin identifiers (variable and method names) so that
the first letter is upper case and all others are lower case. Thus,
for example, the spin file:

    VAR
      x, yy;
    PUB start
      return 0

would create a C++ class with variables "X"  and "Yy" and a function "Start".

The name of the class is taken from the file name. If the base part of
the file name contains more than one capital letter, or has one
capital letter but it is not the first, it is used as the class
name. If the file name is all lower case letters, then the class name
is produced by appending "Spin" to the base of the file name.

Examples:

File name      |  C++ Class name
---------------|------------------
FooBar.spin    |  class FooBar
someFile.spin  |  class someFile
foo99.spin     |  class foo99
foo.spin       |  class fooSpin

In C mode, all the functions have the object name prepended. So
for example in a file named FooBar.spin the "start"
function will be named "FooBar_Start" in the C code.


Annotations and Inline C code
-----------------------------
spin2cpp recognizes special comments called "annotations". Annotations
are Spin comments that start with {++ and end with }. The text between
annotations is passed through to the C++ compiler. This provides a way
to give extra semantic information beyond that available in Spin.

### Variable Annotations

Annotations may appear after variable declarations to associate additional
type specifiers with those variables; for example:

    VAR
      long {++volatile} x

makes `x` a volatile variable in C.

The generated DAT block may similarly have type specifiers associated
with it by placing those after the DAT statement:

    DAT {++volatile}

declares the whole DAT section to be volatile.

### Code Annotations

Whole blocks of C/C++ code may be embedded between `{++` and `}`. Make
sure the '{' and '}' characters are balanced in such code! This
feature is useful for adding additional methods that appear only in C,
or for overriding Spin versions of methods.

If the `{++` appears inside a `PUB` or `PRI` declaration, and it has
been indented, then everything until the next `}` is treated as inline
C code to be inserted directly into the output C or C++. This can be
useful when combined with `#ifdef` to provide C alternatives to Spin
code, or for allowing Spin code to be compiled for another platform.

Code annotations are ignored (treated as comments) if the `--asm` switch
is given.


### Directives

Annotations which begin with the character '!' are special
directives for spin2cpp. The following special directives are
recognized:
     {++!nospin}: do not output any Spin methods
     {++!ccode}:  output C rather than C++ code


COMPILING ON ANOTHER PLATFORM
=============================
It's possible to use spin2cpp together with the `--cc=` option to compile
simple Spin code on a PC. This might make debugging easier. To do this,
it is necessary to `#ifdef` any Propeller specific code and provide generic
C replacements in annotations marked with `{++` and `}`. For example,
a simple serial object that will work on both Propeller and PC could look
like:
```
#ifdef PC
'' we will be using stdio, so force it to
'' be included
{++
#include <stdio.h>
 }
#endif

PUB start(rx_pin, tx_pin, mode, baudrate)
#ifndef PC
  baud := baudrate
  bitcycles := clkfreq / baudrate
  txpin := tx_pin
  txmask := (1<<txpin)
  rxpin := rx_pin
#endif
  return 1
  
PUB tx(c) | val, waitcycles
#ifdef PC
  '' just emit direct C code here
  {++
  putchar(c);
  }
#else
  OUTA |= txmask
  DIRA |= txmask
  val := (c | 256) << 1
  waitcycles := CNT
  repeat 10
     waitcnt(waitcycles += bitcycles)
     if (val & 1)
       OUTA |= txmask
     else
       OUTA &= !txmask
     val >>= 1
#endif
```
The final program would be compiled on e.g. a Linux machine with
the spin2cpp options `--cc=gcc -DPC=1`.

DEVELOPER NOTES
===============
There is a test suite in Test/; to run it do `make test` (this also
builds and runs a simple test program for the lexer). Some of the
tests need to run on the propeller board, so make sure one is plugged
in to a serial port and works with propeller-load (and that
propeller-load is on the path).

Parsing is done via a yacc file (spin.y), but lexing is done with a
hand crafted parser rather than using lex/flex. This is done to make
tracking indentation a little easier.

Mostly the parser builds an Abstract Syntax Tree (AST) which we then
walk to compile the output. Each AST node contains a "kind" (telling
what type of node it is), some immediate data (such as an integer or
string), and left and right pointers. The left and right pointers are
NULL for leaf nodes. Lists are generally represented by a series of
nodes with kind=AST_LISTHOLDER (or similar), with the data pointed to
by ast->left and the rest of the list pointed to by ast->right.

