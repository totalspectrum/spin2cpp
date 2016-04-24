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

Add FCACHE mode for LMM.

Long Term
---------

Implement P2 support.

Implement a spin bytecode back end.

Implement a Tachyon bytecode back end.

Implement a Basic front-end.
