Things to do
============

Short Term Misc
---------------
Support for `_FREE` and `_STACK` (these are read, but ignored at present).

Support inline assembly in C/C++ output.

Implement --data=cog mode (array dereference, mostly).

More documentation.

Code cleanup

Better error messages.

Add some spin2cpp specific defines.

Add an openspin compatible front-end.

Support @@@foo + offset in --asm mode.

Improve the way original Spin code is placed as comments in PASM output.

Optimization
------------

Use djnz for more loops.

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
