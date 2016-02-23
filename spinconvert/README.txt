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

The filename for the output is set automatically by
changing the extension of the Spin file. For example, if the input is
dir\myObj.spin, then the output will be dir\myObj.pasm (or dir\myObj.c
if C output is selected, dir\myObj.cpp for C++). C/C++ code will also
create a header file (.h) automatically; that isn't displayed by the
GUI.


KNOWN ISSUES

Only ASCII or UTF-8 encoded documents can be read and displayed
correctly by the GUI, although the spin2cpp back end can handle
Unicode encoded files.

PASM output cannot handle byte or word variables, or files with the
OBJ directive. It also does notimplement lookup/lookdown, case,
string, coginit/cognew, and some operators. It's still very much a
work in progress. 

The C/C++ output should be pretty complete.
