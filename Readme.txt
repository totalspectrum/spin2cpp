This is a very simple Spin to C++ converter. There is much of the Spin
language that it does not handle, and its results will not always be
correct. 

This version (0.8) includes some more functionality. A simple "Hello,
world" program that compiles and runs is given in the Demo directory,
as are some floating point samples in Demo/Float.

INSTALLATION
============
To install in Windows, just copy spin2cpp.exe to wherever your
propeller-elf-gcc.exe file is installed.

USAGE
=====
spin2cpp is a command line tool. To use it, just give the name of the
.spin file it should convert, e.g:

  spin2cpp test.spin

This will produce a header file test.h and some C++ code test.cpp. It
will also automatically translate any .spin files included as objects,
and produce .cpp and .h files for them as well.

If this is a top level spin object and you plan to compile it, you may
want spin2cpp to automatically insert a main() function and a call to
the first method of the object. To do this, give spin2cpp the --main
option:

  spin2cpp --main test.spin

You can then compile it with:

  propeller-elf-gcc -O -o test.elf test.cpp obj1.cpp obj2.cpp

where "obj1.spin" and "obj2.spin" are the objects referred to by
"test.spin". See Demo/Makefile for examples.

VARIABLE NAMES
==============
Spin is a case-insensitive language, which means that the strings
"str", "Str", "STR", and "sTr" all refer to the same variable. C++, on
the other hand, is case sensitive; all of those strings would be
different variables in C++.

In order to handle this in a consistent way, and to avoid any
conflicts with C++ keywords or library identifiers, spin2cpp
normalizes all Spin identifiers (variable and method names) so that
the first letter is upper case and all others are lower case. Thus,
for example, the spin file:

  VAR
    x, yy;
  PUB start
    return 0

will create a C++ class with variables "X"  and "Yy" and a function "Start".

The name of the class is taken from the file name. If the base part of
the file name contains more than one capital letter, or has one
capital letter but it is not the first, it is used as the class
name. If the file name is all lower case letters, then the class name
is produced by appending "Spin" to the base of the file name.

Examples:

File name:             C++ Class name:
FooBar.spin	       class FooBar
someFile.spin	       class someFile
foo99.spin	       class foo99
foo.spin	       class fooSpin


LIMITATIONS
===========

There are a number of Spin features that are not supported yet,
including:

_CLKMODE
CLKSET
COGINIT on a Spin method (PASM should work)
_FREE
SPR
_STACK
_XINFREQ
arrays of objects in OBJ

There are probably many other features not supported yet!

The lexer and parser are different from the Parallax ones, so they may
well report errors on code the Parallax compiler accepts. For example,
spin2cpp does not handle files that end without a newline very well at
all.

DEVELOPER NOTES
===============
There is a test suite in Test/; to run it do "make test" (this also
builds and runs a simple test program for the lexer). Some of the
tests need to run on the propeller board, so make sure one is plugged
in to a serial port and works with propeller-load (and that
propeller-load is on the path).

Parsing is done via a yacc file (spin.y), but lexing is done with a
hand crafted parser rather than using lex/flex. This is done to make
state dependence a little easier.

Mostly the parser builds an Abstract Syntax Tree (AST) which we then
walk to compile the .cpp and .h files. Each AST node contains a "kind"
(telling what type of node it is), some immediate data (such as an
integer or string), and left and right pointers. The left and right
pointers are NULL for leaf nodes. Lists are generally represented by
a series of nodes with kind=AST_LISTHOLDER (or similar), with the data
pointed to by ast->left and the rest of the list pointed to by
ast->right.

