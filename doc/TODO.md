Things to do
============

Short Term Misc
---------------
Support for `_FREE` and `_STACK` (these are read, but ignored at present).

More documentation.

Code cleanup

Better error messages.

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

Implement a Tachyon bytecode back end.


