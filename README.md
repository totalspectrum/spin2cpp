This is a converter from the Spin language to C/C++. It can deal with most
objects and constructs that are commonly encountered. In fact, it should
be able to deal with almost any Spin program; please report any that it
cannot convert.

A simple "Hello, world" program that compiles and runs is given in the
Demo directory, as are some floating point samples in Demo/Float.

INSTALLATION
============ 
To install in Windows, just unzip the release ZIP file and then copy
spin2cpp.exe to wherever your propeller-elf-gcc.exe file is installed.
(In fact spin2cpp.exe doesn't usually care where it is located, but
putting it with propeller-elf-gcc is convenient.)

USAGE
=====
spin2cpp is a command line tool. To use it, just give the name of the
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
instead of .elf:

    spin2cpp --binary -mcmm -Os test.spin

will create "test.binary", ready to be run with propeller-load or with
any other Spin loader.

If you just want to convert a top level object to C++ (or C), you may
want spin2cpp to automatically insert a main() function and a call to
the first method of the object. To do this, give spin2cpp the --main
option. --main is implied by --elf or --binary, so you do not have to
explicitly give it in those cases.


Examples
--------
spin2cpp is a command line tool, so the examples below are for a CLI
and assume that the appropriate C compilers are in your PATH.

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

OPTIONS
=======
Spin2cpp accepts the following options:

--asm
  Produce (somewhat) readable PASM code as output. This bypasses PropGCC
  altogether. The result may be fed back into spin2cpp and compiled to
  a binary by running spin2cpp --dat --binary.
  
--binary
  Run the compiler and output a loadable binary file. Note that
  this option imples --main. Also note that after --binary you may
  specify options to be passed to PropGCC, such as -Os or -mcmm.

--ccode
  Output C code instead of C++. Note that in C mode methods typically
  have a first parameter "self" which points to the object's data.
  This is similar to the way the C++ compiler implements object methods
  internally, but in C it has to be exposed explicitly.
  
--dat
  Output a binary blob of the DAT section only, similar to the
  bstc -c option; or, if --gas is given, output GAS assembly for
  the DAT section. If --binary is also given, prepends an appropriate
  Spin executable header so the resulting output is executable.

--elf
  Run the compiler and output a linked executable ELF file. Note that
  this option imples --main. Also note that after --elf you may
  specify options to be passed to PropGCC, such as -Os or -mxmmc.

--files
  Print a list of the .cpp (or .c) files that were produced by
  spin2cpp. Useful for tracking object dependencies.

--gas
  Output inline GAS assembly code instead of binary constants. If
  given with the --dat option, produces a .S file containing the
  translation of the PASM code in the file. In other cases, causes
  the DAT section to be compiled into a separate GCC section
  containing inline GAS code. This option is still experimental and
  may not always work correctly.

--main
  Automatically add a C or C++ main() function that will invoke the default
  Spin method. Use this on top level objects only. 

--nopre
  Skip the preprocessor. Normally spin2cpp runs a very simple
  preprocessor on the input.  The pre-processor understands
  #define (of simple macros, no parameters), #undef, #ifdef, #ifndef,
  #else, #elseifdef, #elseifndef, #endif, #include, #error,
  and #warning. Use of the preprocessor should not normally
  cause any issues, but it is still experimental.

--normalize
  Normalize all identifiers so that the first letter is upper case and
  the rest are lower case. This is the way older versions of spin2cpp
  handled identifiers, and is useful for avoiding some identifier
  conflicts. Without this flag, identifiers use the case specified in
  their first occurence.


-Dname=val
  Define a symbol for the preprocessor.

-I path
  Define a path where .spin objects will be searched for. It's OK to use
  this option multiple times.

EXTENSIONS
==========

spin2cpp supports a few extensions to the Spin language:

(1) IF...THEN...ELSE expressions; you can use IF/THEN/ELSE in an expression, like:
   r := if a then b else c
which is the same as
   if a then
     r := b
   else
     r := c

(2) @@@ operator: the @@@ operator returns the absolute hub address of a variable. This is the same as @ in Spin code, but in PASM code @ returns only the address relative to the start of the DAT section. Note that due to implementation issues @@@ works in C/C++ output only if --gas is given.

LIMITATIONS
===========

There are very few Spin features that are not supported yet. _FREE and
_STACK are recognized, but do nothing.

There may be other features that do not work; if you find any,
please report them so they can be fixed.

The lexer and parser are different from the Parallax ones, so they may
well report errors on code the Parallax compiler accepts.

Catalina Issues
---------------
The C code support in spin2cpp is still in very early stages, and so
there are some spin features which are not supported in Catalina (they
do work in PropGCC because the latter supports some C++ extensions
even in C mode).

(1) The LOOKUP and LOOKDOWN functions in Spin do not work in Catalina
unless all the arguments are constant.

(2) The reverse operator will not work in Catalina.

I'm still working on fixing these issues.

C INTEROPERATION
===============

Variable Names
--------------
Spin is a case-insensitive language, which means that the strings
"str", "Str", "STR", and "sTr" all refer to the same variable. C++, on
the other hand, is case sensitive; all of those strings would be
different variables in C++.

Normally spin2cpp will change all instances of an identifier to have the
same case as the first occurence, unless that conflicts with a
built-in C keyword or function; in that case it will change it so
that the first letter is upper case and subsequent letters are lower case.

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

Variable Annotations
--------------------
Annotations may appear after variable declarations to associate additional
type specifiers with those variables; for example:

    VAR
      long {++volatile} x

makes "x" a volatile variable in C.

The generated DAT block may similarly have type specifiers associated
with it by placing those after the DAT statement:

    DAT {++volatile}

declares the whole DAT section to be volatile.

Code Annotations
----------------
Whole blocks of C/C++ code may be embedded between {++ and }. Make
sure the '{' and '}' characters are balanced in such code! This
feature is useful for adding additional methods that appear only in C,
or for overriding Spin versions of methods. At present, it is not
possible to override individual Spin methods; one has to disable all
Spin methods with the {++!nospin} directive (see below) and then write
replacements for all of them in the code annotations.


Directives
----------
Annotations which begin with the character '!' are special
directives for spin2cpp. The following special directives are
recognized:
     {++!nospin}: do not output any Spin methods
     {++!ccode}:  output C rather than C++ code


DEVELOPER NOTES
===============
There is a test suite in Test/; to run it do "make test" (this also
builds and runs a simple test program for the lexer). Some of the
tests need to run on the propeller board, so make sure one is plugged
in to a serial port and works with propeller-load (and that
propeller-load is on the path).

Parsing is done via a yacc file (spin.y), but lexing is done with a
hand crafted parser rather than using lex/flex. This is done to make
tracking indentation a little easier.

Mostly the parser builds an Abstract Syntax Tree (AST) which we then
walk to compile the .cpp and .h files. Each AST node contains a "kind"
(telling what type of node it is), some immediate data (such as an
integer or string), and left and right pointers. The left and right
pointers are NULL for leaf nodes. Lists are generally represented by
a series of nodes with kind=AST_LISTHOLDER (or similar), with the data
pointed to by ast->left and the rest of the list pointed to by
ast->right.

