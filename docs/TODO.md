Things to do
============

Short Term Misc
---------------
Support for `_FREE` and `_STACK` (these are read, but ignored at present).

Support @@@ in --asm mode.

Support inline assembly in C/C++ output.

Add original Spin code as comments in PASM output.

Implement --data=cog mode (array dereference, mostly).

More documentation.

Code cleanup

Better error messages.

Add some spin2cpp specific defines.

Add an openspin compatible front-end.

Optimization
------------

Hoist loop invariant assignments out of loops.

Optimize rdlong/rdlong wrlong/rdlong sequences.

Loop strength reduction, particularly for address calculations.

Common subexpression elimination.

Take advantage of commutative operators to improve optimization.

Long Term
---------

Implement a spin bytecode back end.

Implement a Tachyon bytecode back end.

Implement a Basic front-end.
