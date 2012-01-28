This is a very simple Spin to C++ converter. There is much of the Spin
language that it does not handle, and its results will not always be
correct. 

This version (0.5) includes some more functionality. A simple "Hello,
world" program that compiles and runs is given in the Demo directory,
as are some floating point samples in Demo/Float.

spin2cpp is a command line tool. To use it, just give the name of the
.spin file it should convert, e.g:

  spin2cpp test.spin

This will produce a header file test.h and some C++ code test.cpp. It
will also automatically translate any .spin files included as objects,
and produce .cpp and .h files for them as well.

If this is a top level spin object and you plan to compile it, you may
want spin2cpp to automatically insert a main() function and a call to
the first method of the object. To do this, give spin2cpp the -main
option:

  spin2cpp -main test.spin

You can then compile it with:

  propeller-elf-gcc -O -o test.elf test.cpp obj1.cpp obj2.cpp

where "obj1.spin" and "obj2.spin" are the objects referred to by
"test.spin". See Demo/Makefile for examples.


LIMITATIONS
===========

There are a number of Spin features that are not supported yet,
including:

_CLKMODE
CLKSET
COGINIT on a Spin method (PASM should work)
_FREE
LOOKDOWN and LOOKDOWNZ
SPR
_STACK
_XINFREQ
arrays of objects in OBJ
operators:
  the ^^ operator is not supported at all
  the ? operator is not properly supported (it will return a random
  number,but will not change the variable it is applied to)
using reversed ranges in the pins field affecting a hardware register
  (e.g. OUTA[12..7] should work, but OUTA[7..12] will not)

There are probably many other features not supported yet!

The lexer and parser are different from the Parallax ones, so they may
well report errors on code the Parallax compiler accepts. For example,
spin2cpp does not handle files that end without a newline very well at
all.

DEVELOPER NOTES
===============
There is a test suite in Test/; to run it do "make test" (this also
builds and runs a simple test program for the lexer).

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

