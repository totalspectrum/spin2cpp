Spin2cpp is a program for converting Spin, BASIC, or C code to binary, PASM (Parallax Propeller 1 assembly), P2ASM (Parallax P2 assembly), Spin1 bytecode, C, or C++.

flexspin is a slightly different front end to the same functionality, designed to be compatible with other spin compilers.

flexcc is a third front end designed to accept the arguments normally given to C compilers (`cc`). Like the others, it can actually accept Spin and BASIC input as well as C.

The command line options for spin2cpp and flexspin are documented in Spin2cpp.md and Flexspin.md respectively.

Documentation for the various front ends (languages accepted as input) are in doc. The BASIC language is the best documented. The C and Spin documentation just cover extensions and deviations from the standard (C99 in the case of C, and the original Propeller compiler in the case of Spin).


## BUILDING

On most Linux installations you should be able to build with "make", which will place binaries in the "build" folder.

On macOS the procedure is similar, but you'll probably have to manually install a newer version of bison (the one that comes with macOS is too old). You'll need at least bison 3.0.

On Windows it should be possible to build using msys or a similar gcc based build environment, but you'll need to install bison 3.0 or later, flex, gcc, and make.

## Tips and Tricks

### Executable Name

If you name your executable "bstc.exe", it will try to act like the BSTC compiler (so it can be used in build scripts and IDEs that use bstc style command lines). For example, flexspin will work this way as the back-end assembler for PropBasic.

If you name your executable something that ends in "spin2" (like "flexspin2.exe") it will compile for P2 by default.

## Credits

Most of spin2cpp was written by Eric R. Smith. The bytecode back end and many other improvements and bug fixes were contributed by Ada Gottensträter, and she has contributed many helpful suggestions. The C preprocessor was written by Kiyoshi Matsui. Many people have contributed bug fixes, suggestions, and library files; see the COPYING and COPYING.LIB files for the full list of contributors.

Some of the work on spin2cpp was done thanks to Eric's employer, Collabora Ltd. Many thanks to them for their support of open source projects including this one.
