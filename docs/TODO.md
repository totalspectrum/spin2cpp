Things to do
============

Short Term Misc
---------------
Support for `_FREE` and `_STACK` (these are read, but ignored at present).

Support inline assembly in C/C++ output.

More documentation.

Code cleanup

Better error messages.

Add in the missing openspin options to fastspin.

Support @@@foo + offset in --asm mode.

Optimization
------------

Hoist loop invariant assignments out of loops.

Loop strength reduction, particularly for address calculations.

Common subexpression elimination.

Take advantage of commutative operators to improve optimization.

Put more commonly used functions in COG RAM, and/or FCACHE whole functions
if they are called a lot.

Allow system functions to go into HUB RAM to free up COG space.

Better fcache loading code.

Faster multiply / divide.

Long Term
---------

Finish P2 support.

Implement a spin bytecode back end.

Implement a Tachyon bytecode back end.

Implement a Basic front-end.
