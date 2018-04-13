OVERVIEW
========

Fastspin is a compiler from the Spin language to assembly (PASM).
Normally Spin is translated to a special kind of bytecode which
is interpreted by a program in the Propeller's ROM. This bytecode
is very compact, but it is slow to run. Fastspin produces much larger
code, but it is also much faster.

Fastspin is an alternate (simpler) frontend to spin2cpp. It does not
support translation to C or C++, only to binary (via PASM). The
command line options for fastspin are very similar to those of the
openspin compiler:

The basic usage is very simple:

   fastspin program.spin

will compile program.spin into program.binary, which may then be
loaded into the Propeller. There are various command line options
which may modify the compilation:

  [ -h ]              display this help
  [ -L or -I <path> ] add a directory to the include path
  [ -o ]             output filename
  [ -b ]             output binary file format
  [ -e ]             output eeprom file format
  [ -c ]             output only DAT sections
  [ -f ]             output list of file names
  [ -q ]             quiet mode (suppress banner and non-error text)
  [ -p ]             disable the preprocessor
  [ -2 ]             compile for Prop2
  [ -O ]             enable additional optimizations
  [ -D <define> ]    add a define

The -2 option is new: it is for compiling for the Propeller 2 (FPGA
version). The output code may not may not work properly on any given
FPGA image, because the Prop2 is a moving target.

fastspin.exe checks the name it was invoked by. If the name starts
with the string "bstc" (case matters) then its output messages mimic
that of the bstc compiler; otherwise it tries to match openspin's
messages. This is for compatibility with Propeller IDE. For example,
you can use fastspin with the PropellerIDE by renaming bstc.exe to
bstc.orig.exe and then copying fastspin.exe to bstc.exe.

EXTENSIONS
==========

fastspin supports a few extensions to the Spin language:

(1) fastspin has a pre-processor that understands `#include`, `#define`, and
`#ifdef / #ifndef / #else / #endif`. There are several predefined symbols:

Symbol           | When Defined
-----------------|-------------
`__FASTSPIN__`   | if the `fastspin` front end is used
`__SPINCVT__`    | always defined
`__SPIN2PASM__`  | if --asm is given (PASM output) (always defined by fastspin)
`__SPIN2CPP__`   | if C++ or C is being output (never in fastspin)
`__cplusplus`    | if C++ is being output (never in fastspin)
`__P2__`         | if compiling for Propeller 2

(this isn't exactly an extension anymore, since openspin has the same
preprocessor).

(2) @@@ operator: the @@@ operator returns the absolute hub address of
a variable. This is the same as @ in Spin code, but in PASM code @
returns only the address relative to the start of the DAT section.

(3) IF...THEN...ELSE expressions; you can use IF/THEN/ELSE in an expression, like:
```
r := if a then b else c
````
which is the same as
```
   if a then
     r := b
   else
     r := c
```

(4) fastspin accepts inline assembly in PUB and PRI sections. Inline
assembly starts with `asm` and ends with `endasm`. The inline assembly
is still somewhat limited; the only operands permitted are local
variables of the containing function.

(5) The proposed Spin2 syntax for abstract object definitions and object pointers is accepted. A declaration like:
```
OBJ
  fds = "FullDuplexSerial"
```
declares `fds` as having the methods of a FullDuplexSerial object, but without any actual variable or storage being instantiated. Symbols declared this way may be used to cast parameters to an object type, for example:
```
PUB print(f, c)
  fds[f].dec(c)

PUB doprint22
  print(@aFullDuplexSerialObj, 22)
```

CODE
====

The output code and data are always placed in HUB memory, and the code is
interpreted with a simple LMM interpreter. The spin2cpp front end has
additional options (for example to place code in COG memory).

In P2 the code is still placed in HUB memory, but no LMM interpreter is
needed (we use the P2 hubexec feature).

LIMITATIONS
===========

Not all P2 assembly instructions and addressing modes are supported yet.

Beware when compiling P1 objects that contain PASM for P2: some
instructions have changed in subtle ways.

Programs compiled with fastspin will always be larger than those
compiled with openspin or bstc. If the spin code is mostly PASM (as is
the case with, for example, PropBASIC compiler output) then the
fastspin overhead will be relatively small and probably fixed, but for
large programs with lots of Spin methods the difference could be
significant.
