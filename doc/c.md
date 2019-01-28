# Flex C (the C dialect accepted by fastspin)

## DEVELOPMENT STATUS

C compiler support is not even at the "beta" stage yet; there are many features missing.

### Missing Features

An incomplete list of things that still need to be implemented:

  * bitfields
  * variable scope inside statement blocks (right now all local variables in a function share the same scope)
  * 64 bit integers (long long)
  * struct passing and return
  * struct initialization
  * most of the C standard library
  
## Introduction

## Preprocessor

Flex C uses the open source mcpp preprocessor (originally from mcpp.sourceforge.net), which is a very well respected and standards compliant preprocessor.

### Predefined symbols

Symbol           | When Defined
-----------------|-------------
`__propeller__`  | always defined to 1 (for P1) or 2 (for P2)
`__FLEXC__`      | always defined to the fastspin version number
`__FASTSPIN__`   | always defined to the fastspin version number
`__cplusplus`    | if C++ is being output (not currently implemented)
`__P2__`         | only defined if compiling for Propeller 2

## Extensions to C

### Inline Assembly

The inline assembly syntax is similar to that of MSVC. Inline assembly blocks are marked with the keyword `__asm`. For example, a function to get the current cog id could be written as:
```
int getcogid() {
   int x;
   __asm {
      cogid x
   };
   return x;
}
```
The `__asm` keyword must be followed by a `{`; everything between that and the next `}` is taken to be assembly code.

Inside inline assembly any instructions may be used, but the only legal operands are integer constants and local variables (or parameters) to the function which contains the inline assembly. Labels may be defined, and may be used as the target for `goto` elsewhere in the function.

### External Classes (e.g. Spin Objects)

It is possible to use classes written in other languages. The syntax is similar to the BASIC `class using`, but in C this is written `struct __using`. For example, to use the FullDuplexSerial Spin object you would do:
```
struct __using("FullDuplexSerial.spin") fds;

void main()
{
    fds.start(31, 30, 0, 115_200);
    fds.str("hello, world!\r\n");
}
```
This declares a struct `fds` which corresponds to a Spin OBJ, using the code in "FullDuplexSerial.spin". Spin, BASIC, and even C code may be used. In the case of C code, something like:
```
struct __using("myclass.c") myclass;
```
is basically equivalent to:
```
struct {
#include "myclass.c"
} myclass;
```
Note that allowing function definitions inside a struct is an extension to C (it is feature of C++).

### Header file external function definitions

There is no linker as yet, so in order to use standard library functions we use a FlexC specific construct, `__fromfile`. The declaration:
```
  size_t strlen(const char *s) __fromfile("libc/string/strlen.c");
```
declares the `strlen` function, and also says that if it is used and no definition is given for it, the file "libc/string/strlen.c" should be added to the build. This file is searched for along the standard include path.

## Builtin functions

### ABS

```
  x = __builtin_abs(y)
```
Calculates the absolute value of `y`. This is not like a normal C function in that the result type depends on the input type. If the input is an integer, the result is an integer. If the input is a float, the result is a float.

### ALLOCA

```
  ptr = __builtin_alloca(size)
```
Allocates `size` bytes of memory on the stack, and returns a pointer to that memory. When the enclosing function returns, the allocated memory will become invalid (so do not attempt to return the result from a function!)

### COGSTART

Starts a function running in another COG. This builtin is more of a macro than a traditional function, because it does not immediately evaluate its first parameter (which should be a function call); instead, it causes that function call to run in a new COG. For example:
```
  static long stack[32];
  id = __builtin_cogstart(somefunc(a, b), &stack[0]);
```
runs `somefunc` with parameters `a` and `b` in a new COG, using the given stack space.

The amount of space required for the stack depends on the complexity of the code to run, but must be at least 16 longs (64 bytes).

`__builtin_cogstart` returns the identifier of the new COG, or -1 if no COGs are free.

### REV

```
x = __builtin_rev(y, n)
```
Reverses the bits of `y` and then shifts the result right by `32-n` places. This effectively means that the bottom `n` bits of `y` are reversed, and 0 placed in the remaining bits.

### SQRT

```
  x = __builtin_sqrt(y)
```
Calculates the square root of `y`. This is not like a normal C function in that the result type depends on the input type. If the input is an integer, the result is an integer. If the input is a float, the result is a float.

