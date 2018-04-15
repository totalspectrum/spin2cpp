Automatically Converting Spin To Pasm
=====================================

Introduction
------------

Spin is the standard programming language for the Propeller. It's
generally implemented by means of a bytecode compiler on the PC,
producing binaries that are interpreted by a bytecode interpreter in
the Propeller ROM. This works well -- Spin programs are compact, and
perform reasonably well. But sometimes you need more performance than
an interpreter can give.

The fastspin compiler can convert Spin programs to PASM
automatically. Originally fastspin only worked on whole programs,
which was fine if you had a small amount of code to speed up.