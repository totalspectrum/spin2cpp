Spin Converter
Copyright 2011-2016 Total Spectrum Software Inc.
See COPYING for terms of redistribution

This is a simple Spin to PASM (or C/C++) converter. This particular
release is a "preview", which means it is incomplete and still
buggy. It won't handle all Spin programs correctly. Please check the
output and be prepared to fix problems.

USAGE

Select the type of output you wish to get from the Options menu (the
default is PASM), then use File>Open to open your Spin file. The Spin
source code will appear on the left, and the converted file on the
right. The converted file is updated automatically whenever the Spin
source is saved.

If you select PASM output then you have the option to produce an
executable .binary. To do this select the "Make binary" option under
the Options menu. This only works for PASM output. The result has the
same name as the PASM output file, but with an extension of
".binary". To run this binary file on your Propeller hardware you will
have to use the Propeller Tool or some similar IDE, or use the
propeller-load command line tool from PropGCC.

The filename for the output is set automatically by
changing the extension of the Spin file. For example, if the input is
dir\myObj.spin, then the output will be dir\myObj.pasm (or dir\myObj.c
if C output is selected, dir\myObj.cpp for C++). C/C++ code will also
create a header file (.h) automatically; that isn't displayed by the
GUI.


KNOWN ISSUES

PASM output cannot handle byte or word variables, or files with the
OBJ directive. It also does notimplement lookup/lookdown, case,
coginit/cognew, and some operators. It's still very much a
work in progress. 

The C/C++ output should be complete.
