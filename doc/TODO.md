Things to do
============

C Improvements
--------------
stdio FILE routines and vfs

__attribute__ for constructors and cogexec

Finish porting simpletools.h

BASIC Improvments
-----------------
Remaining math functions

Remaining string functions

Easier implicit declarations

Short Term Misc
---------------
More documentation.

Code cleanup

Better error messages.

Support for `_FREE` and `_STACK` (these are read, but ignored at present).

Add in the missing openspin options to fastspin.

Add an option to get a report of method sizes and free space left.

Optimization
------------

Put more commonly used functions in COG RAM, and/or FCACHE whole functions
if they are called a lot.

Better fcache loading code.


Long Term
---------

Implement a spin bytecode back end.

Implement a Python-like front end.
