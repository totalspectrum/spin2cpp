Things to do
============

C Improvements
--------------
__attribute__ for constructors and cogexec

Finish porting simpletools.h

virtual methods?

allow method declarations outside classes

C++ lambdas

Implement "long long"

Implement variable sized local arrays

#pragma lib

better flexcc frontend


BASIC Improvments
-----------------
Remaining math functions

Remaining string functions

Easier implicit declarations

Spin
----
Update to match spin2

Float operators:
  ABS.
  SQRT.
  *. /. +. -.
  #>. #<.  (force ge or le)
  <. <=. >=. >.
  <=>.  return -1, 0, 1
  a := float(i)             ' convert i from integer to float
  i := round(a) or trunc(a) ' convert float back to int
  
Short Term Misc
---------------
More documentation.

Code cleanup

Better error messages.

Add in the missing openspin options to flexspin.

Add an option to get a report of method sizes and free space left.

full public/private support

Optimization
------------

Put more commonly used functions in COG RAM, and/or FCACHE whole functions
if they are called a lot.

Better fcache loading code.


Long Term
---------

Implement a spin bytecode back end.

Implement a Python-like front end.
