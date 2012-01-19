This is a very simple Spin to C++ converter. There is much of the Spin
language that it does not handle, and its results will not always be
correct. 

This version (0.4) includes some more functionality. A simple "Hello,
world" program that compiles and runs is given in the Demo directory,
as are some floating point samples in Demo/Float.

spin2cpp is a command line tool. To use it, just give the name of the
.spin file it should convert, e.g:

  spin2cpp test.spin

This will produce a header file test.h and some C++ code test.cpp. It
will also automatically translate any .spin files included as objects,
and produce .cpp and .h files for them as well.

If this is a top level spin object and you plan to compile it, you may
want spin2cpp to automatically insert a main() function and a call to
the first method of the object. To do this, give spin2cpp the -main
option:

  spin2cpp -main test.spin

You can then compile it with:

  propeller-elf-gcc -O -o test.elf test.cpp obj1.cpp obj2.cpp

where "obj1.spin" and "obj2.spin" are the objects referred to by
"test.spin". See Demo/Makefile for examples.

