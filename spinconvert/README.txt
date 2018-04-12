Spin Converter
==============
Copyright 2011-2018 Total Spectrum Software Inc.
See COPYING for terms of redistribution

This is a simple Spin to PASM (or C/C++) converter. It should be able
to translate all Spin programs, but obviously we cannot guarantee
that. Please check the output and be prepared to fix problems.

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
".binary". To run this binary file on your Propeller hardware you can
use the Propeller Tool or some similar IDE, or use the
propeller-load command line tool from PropGCC. Alternatively, you
can select "Run on device" from the "Run" menu, which will launch
propeller-load automatically.

Also under the Options menu are selection for "PASM Options..." and
"C/C++ Options...".

The PASM options dialog box contains selectors for whether code goes
in Cog memory or Hub memory. If the code goes in Hub memory then you
can run larger Spin programs (the output doesn't have to fit in the
tiny Cog memory!) but comes at a cost: reading instructions from Hub
memory slows the program down by about 4x. There's also a selection
for placing data in Cog or Hub memory; normally data should go in Hub
memory, as doing byte or word access in Cog memory is not possible.

Under C/C++ Options are selections for how to handle the DAT section
(converting PASM assembler syntax to GAS syntax, or else compiling the
DAT to an array of bytes 'binary blob') and how to handle variable
names (Spin is case insensitive, whereas C is case sensitive, so all
Spin variable names have to be converted to some standard form; either
the case the variable is spelled with originally, or a "normalized"
form as described in the spin2cpp documentation). There is also a
checkbox asking spin2cpp to try to guess at C types; without this the
compiler will use int32_t for all types, even pointers.

The filename for the output is set automatically by
changing the extension of the Spin file. For example, if the input is
dir\myObj.spin, then the output will be dir\myObj.pasm (or dir\myObj.c
if C output is selected, dir\myObj.cpp for C++). C/C++ code will also
create a header file (.h) automatically; that isn't displayed by the
GUI.

Menus
-----

### File

#### New Spin file

Creates a new empty Spin file for you to edit.

#### Open Spin

Brings up a file selector so you can open an existing Spin file.

#### Save Spin

Saves the current Spin file, and compiles it automatically (so the
"Converted Code" and "Compiler Output" windows are updated).

#### Save Spin As

Like "Save Spin", but also allows you to change the name of the Spin
file. This option also triggers recompilation of the Spin file and
update of all windows.

#### Exit

Quits the Spinconvert program.

### Edit

This menu has the usual Cut/Copy/Paste options. These apply to the
current window (the last one that was clicked on).

### Options

#### Output options

The "PASM Output", "C output", and "C++ Output" entries select what
kind of output will be produced. They are mutually exclusive. When the
output type is changed, the "Converted Code" window will automatically
be updated, but only if the "Original Spin" has been saved to disk.

#### Make Binary

This is a toggle; if a checkmark appears in the menu entry then it is
enabled. If the option is enabled then the compiler is run to produce a
binary file from the converted code (PASM, C, or C++). PASM code may
always be converted to binary, since spinconvert comes with a
compiler for PASM. C and C++ code use propeller-elf-gcc as a compiler,
so PropGCC must be installed and its bin directory must be in the
Windows PATH environment variable.

#### PASM options...

This brings up a dialog giving options for PASM output.
With PASM output, code may be placed in COG memory or in HUB memory.
If the PASM code is placed in HUB memory it must be loaded
into COG memory by a tiny interpreter (the "LMM kernel"). This causes
the code to run more slowly, but allows for much larger programs to
run.

Similarly, data may be placed either in COG or HUB memory. By "data"
we mean variables and the run time stack. The DAT section is always
placed in HUB memory, as are any strings.

The default for both code and data is to be placed in HUB memory,
since that is much larger.

#### Library directory

The Library directory is a place where the converter looks for Spin
objects if they cannot be found in the current directory.


### Run

#### Run on device

Compiles to binary (so it forces the "Make Binary" selection to true)
and then runs the resulting program on a Propeller device plugged in
to the computer. If more than one Propeller is plugged in then results
are very unpredictable (i.e. it will probably fail). A terminal
program is opened up at 115200 baud to display output from the
program. The baud rate and other settings are not currently changable.

KNOWN ISSUES
------------

The C/C++ output should be complete.

The PASM output also seems to be complete, but is not as thoroughly
tested, so it is likely to have some bugs.

The "Run on device" option is very limited; it will fail if more than
one device is plugged in, and it will only display output at 115200
baud. Someday it would be nice to be able to customize this.


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

