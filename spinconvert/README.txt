Spin Converter
==============
Copyright 2011-2016 Total Spectrum Software Inc.
See COPYING for terms of redistribution

This is a simple Spin to PASM (or C/C++) converter. This particular
release is a "preview", which means it is incomplete and still
buggy. It won't handle all Spin programs correctly. Please check the
output and be prepared to fix problems.

USAGE
-----

Select the type of output you wish to get from the Options menu (the
default is PASM), then use File>Open to open your Spin file. The Spin
source code will appear on the left, and the converted file on the
right. The converted file is updated automatically whenever the Spin
source is saved. The original Spin source code should appear as
comments within the converted PASM code (this is only for PASM; C is
close enough to Spin that the conversion should be pretty obvious).

If you select PASM output then you have the option to produce an
executable .binary. To do this select the "Make binary" option under
the Options menu. This always works for PASM output, and will work
for C/C++ output only if you have PropGCC installed. The result has the
same name as the PASM output file, but with an extension of
".binary". To run this binary file on your Propeller hardware you will
have to use the Propeller Tool or some similar IDE, or use the
propeller-load command line tool from PropGCC.

Also under the Options menu is a selection for "LMM mode". If "LMM mode"
is on, then the PASM output is run in "Large Memory Model", which
means that the code is actually placed in Hub memory. This means you
can run larger Spin programs (the output doesn't have to fit in the
tiny Cog memory!) but comes at a cost: reading instructions from Hub
memory slows the program down by about 4x.

The filename for the output is set automatically by
changing the extension of the Spin file. For example, if the input is
dir\myObj.spin, then the output will be dir\myObj.pasm (or dir\myObj.c
if C output is selected, dir\myObj.cpp for C++). C/C++ code will also
create a header file (.h) automatically; that isn't displayed by the
GUI.


KNOWN ISSUES
------------

The C/C++ output should be complete.

The PASM output also seems to be complete, but is not as thoroughly
tested, so it is likely to have some bugs.

EXTENSIONS TO THE SPIN LANGUAGE
-------------------------------

The Spin Converter can actually accept a few extensions to Spin, at
least in some circumstances.

### Preprocessor

spin2cpp supports #ifdef / #ifndef / #else / #endif and
#include. These preprocessor defines have been widely implemented in
other Spin compilers such as bstc, homespun, and openspin, but are not
present in the original Propeller Tool.

#### Predefined Symbols

The preprocessor always defines the symbol `__SPIN2X__`.

If the PASM output is selected, it defines `__SPIN2PASM__`.

If C or C++ output is selected, it defines `__SPIN2CPP__`. For C++
output it also defines `__cplusplus`.

### Absolute Addresses

The @@@ operator for absolute hub address is accepted in DAT
sections if they are being compiled to binary (--dat --binary) or if
GAS output is given (--gas). There are technical issues that make
supporting it in other circumstances more difficult, but I hope to
be able to do so eventually,

### Conditional Expressions

IF/THEN/ELSE expressions are supported, for example:

    x := if a<b then 0 else 1

will assign x the value of 0 if a is less than b, 1 otherwise. This is
very much like C's (? :) ternary operator. The compiler uses this
internally for some things, and for testing purposes it was useful to
change the syntax to make it available.

### Inline Assembly

Inline assembly in Spin functions is supported in Spin functions,
between lines starting "asm" and "endasm". The assembly is very
limited; only local variable names (including parameters) and
immediate constants are legal operands for instructions. Still, it can
be useful, and is in fact heavily used internally for built in
operators. For example, the lockclr Spin function is implemented as:

```
pub lockclr(id) | mask, rval
  mask := -1
  asm
    lockclr id wc
    muxc rval, mask
  endasm
  return rval
```

At the moment inline assembly only works for PASM output, it is not
yet supported in C or C++.

