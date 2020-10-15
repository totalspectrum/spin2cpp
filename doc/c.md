# Flex C

## Introduction

FlexC is the C dialect implemented by the fastspin compiler used in FlexGUI. It eventually will implement the C99 standard with some C++ extensions.

fastspin recognizes the language by the extension of the file being compiled. If a file ends in `.c` it is treated as a C file. If a file ends in `.cpp`, `.cc`, or `.cxx` then it is treated as a C++ file; this enables a few keywords not available in C, but otherwise is very similar to C mode (FlexC is not a full featured C++ compiler).

This document assumes that you are familiar with programming in C and with the Parallax Propeller chip. It mostly covers the differences between standard C and FlexC.

## DEVELOPMENT STATUS

The C compiler is mostly implemented and could probably be considered "beta" software now, but there are a few missing features.

### Known Bugs

There are several known bugs and deviations from the C99 standard:

#### Name Spaces

The namespaces for types and variable names are not separated as they should be, so some C code that uses the same identifiers for types and variables or struct members may not work properly.

#### Doubles

The `double` type is implemented as a 32 bit IEEE single precision float (the same as `float`). This doesn't meet the requirements in the C99 and later standards for the range available for double.

#### long long

The 64 bit integer type ("long long") is only partially implemented at this time, and does not work properly.

#### Designated initializers

C99 desginated initializers are supported only in their simplest form, that is, for only one level of initializer. So for example a statement like:
```
  struct point c = { .x = 1, .y = 2 };
```
will work, but a designated initializer for a sub structure field like:
```
  struct person p = { .address.streetnum = 10 };
```
will not work: the double levels of ".address.streetnum" will fail.

#### Anonymous Structs and Unions

Anonymous structs and unions (i.e. nested struct members without a name) are not supported.

#### Variable length arrays

Variable length arrays are not supported.

## Preprocessor

Flex C uses the open source mcpp preprocessor (originally from mcpp.sourceforge.net), which is a very well respected and standards compliant preprocessor.

### Predefined symbols

Symbol           | When Defined
-----------------|-------------
`__propeller__`  | always defined to 1 (for P1) or 2 (for P2)
`__FLEXC__`      | always defined to the fastspin major version number
`__FASTSPIN__`   | always defined to the fastspin major version number
`__P2__`         | only defined if compiling for Propeller 2 (obsolete)
`__propeller2__` | only defined if compiling for Propeller 2
`__ILP32__`      | always defined; some programs use this to determine pointer size
`__VERSION__`    | defined to a string containing the full fastspin version

## Runtime Environment

### P1 Clock Frequency

In C code, the P1 clock frequency defaults to 80 MHz, assuming a 5 MHz crystal and `xtal1 + pll16x` clock mode. This is a common configuration. You may override it with the builtin `clkset` function, which works the same as the Spin `clkset` builtin.

### P2 Clock Frequency

The P2 does not have a default clock frequency. You may set up the frequency with the loader (loadp2), but it is probably best to explicitly set it using `_clkset(mode, freq)`. This is similar to the P1 `clkset` except that `mode` is a P2 `HUBSET` mode.

Header files `sys/p2es_clock.h` and `sys/p2d2_clock.h` are provided for convenience in calculating a mode. To use these, define the macro P2_TARGET_MHZ before including the appropriate header file for your board. The header will calculate and define macros `_SETFREQ` (containing the mode bits) and `_CLOCKFREQ` (containing the frequency; this should normally be `P2_TARGET_MHZ * 1000000`). So for example to set the frequency to 160 MHz you would do:
```
#define P2_TARGET_MHZ 160
#include <sys/p2es_clock.h>
...
_clkset(_SETFREQ, _CLOCKFREQ);
```
The macros `_SETFREQ` and `_CLOCKFREQ` are not special in any way, and this whole mechanism is just provided as a convenience. You may completely ignore it and calculate the mode bits and frequency setting to pass to `_clkset` yourself.

## Extensions to C

### Inline Assembly (C Style)

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
The `__asm` keyword must be followed by a `{` or else `const` (or `volatile`) and then a `{`; everything between that and the next `}` is taken to be assembly code. `__asm volatile` suppresses optimization of the assembly code (see below) and forces the code to be placed into FCACHE memory. `__asm const` is similar, but does not force the code into FCACHE (it will execute from HUB as usual).

For inline assembly inside a function, any instructions may be used, but the only legal operands are integer constants (preceded by `#`) and local variables, including parameters, of the function which contains the inline assembly. Labels may be defined, and may be used as the target for `goto` elsewhere in the function.

Some conditional execution directives (like `if_c_and_z`) are not accepted in inline assembly. In general, inline assembly is restricted, and is no substitute for full assembly in top level `__pasm` blocks.

Inline assembly inside a function is normally optimized along with the generated code; this produces opportunities to improve the generated code based on knowledge of the assembly. This may be suppressed by using `__asm const` (or `__asm volatile`) instead of `__asm`. Generally this will hurt the output code, but may be necessary if there is inline assembly with very sensitive timing.

Inline assembly may also appear outside of any function. In this case the inline assembly block is similar to a Spin `DAT` section, and creates a global block of code and/or data.

The syntax of expressions inside inline assembly is the same as that of C, and only C style constants may be used (so write `0xff` instead of `$ff`). Comments must be made in C style, not Spin style (so use `//` to begin a comment that extends to the end of line).

### Inline Assembly (Spin Style)

Because much existing assembly code is written in the Spin language, FlexC supports inline assembly that (mostly) uses the Spin rules for expression evaluation and comments. These blocks are like C style inline assembly, but start with the keyword `__pasm` instead of `__asm`. Inside `__pasm` blocks comments start with a single quote, and expressions are evaluated as they are in Spin.

`__pasm` blocks may only appear at top level (outside of any function). Inside a function only `__asm` blocks are supported, for now. Also note that the Spin language compatibility inside `__pasm` blocks is still a work in progress, and there are probably many missing pieces.

Note that FlexC supports calling Spin methods directly, so to adapt existing Spin code it may be easier to just include the Spin object with `struct __using` (see below).

### Classes

The C `struct` declaration is extended slightly to allow functions (methods) to be declared. All methods must be defined in the struct definition itself; there is at present no way to declare a method outside of the definition.

For example, a simple counter class might be implemented as:
```
typedef struct counter {
  int val;
  void setval(int x) {
      val = x;
  }
  int getval() { return val; }
  int incval() { return ++val; }
} Counter;

Counter x;
...
x.setval(0);
x.incval();
```

In C++ mode (that is, if the file being compiled has an extension like `.cpp` or `.cc`) then the keyword `class` may be used instead of `struct`. At the moment `class` and `struct` are treated identically, although eventually they will have different defaults for whether members are public or private.

The keywords `public` and `private` are recognized by the parser, but currently do nothing (all members are public).

### External Classes (e.g. Spin Objects)

It is possible to use classes written in other languages. The syntax is similar to the BASIC `class using`, but in C this is written `struct __using`. For example, to use the FullDuplexSerial Spin object you would do:
```
struct __using("FullDuplexSerial.spin") fds;

void main()
{
    fds.start(31, 30, 0, 115200);
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

#### Name resolution in Spin and BASIC classes

Because Spin and BASIC are case insensitive languages, their identifiers may be accessed in a case insensitive way (e.g. `x.Vga`, `x.VGA`, and `x.vga` are all equivalent if `x` is a Spin or BASIC class. It is strongly recommended to be consistent though, because this will avoid confusion for readers who are used to C being a case sensitive language.

#### RESTRICTIONS ON C CLASSES

Class support for C/C++ is very much incomplete, and probably will not work in all cases. In particular, calling functions outside of the class may not work (so simple self contained classes should be fine, but classes which call into other classes may not work properly).

### Header file external function definitions

There is no linker as yet, so in order to use standard library functions we use a FlexC specific construct, `__fromfile`. The declaration:
```
  size_t strlen(const char *s) __fromfile("libc/string/strlen.c");
```
declares the `strlen` function, and also says that if it is used and no definition is given for it, the file "libc/string/strlen.c" should be added to the build. This file is searched for along the standard include path.

### C++ reference parameters

FlexC supports C++ references. These are just like pointers, but are automaticaly dereferenced upon use. They are declared with `&` in place of `*`. For example, a function to swap two integers could be written as:
```
void swap(int &a, int &b)
{
   int t = a;
   a = b;
   b = t;
}
```
and used as
```
   swap(x, y);
```
Internally this is the same as the traditional C:
```
void swap_c(int *a, int *b)
{
   int t = *a;
   *a = *b;
   *b = t;
}
...
   swap_c(&x, &y);
```

### Default values for parameters

Like C++, FlexC allows function parameters to be given default values. For example, if a function is declared as:
```
int incr(int x, int v=1) { return x + v; }
```
then a call `incr(x)` where the second parameter is not given will be compiled as `incr(x, 1)`.

### __this and __class

The keywords `__this` and `__class` are allowed in both C and C++ code, and mean the same as `this` and `class` in C++. These keywords are intended for use in C code which wishes to use FlexC's class features.

### Statement expressions

A compound statement enclosed in parentheses may appear as an expression. This is a GCC extension which allows loops, switches, and similar features to appear within an expression.

### Miscellaneous extensions

The `@` symbol may be used as an addressof operator in place of `&`. This is mainly useful in inline assembly (where it mimics the syntax of PASM/Spin).

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

### Count Leading Zeros

```
  x = __builtin_clz(y)
```
Calculates the number of 0 bits at the start of the unsigned integer `y`. The result is between 0 and 32 (inclusive).

### COGSTART

Starts a function running in another COG. This builtin is more of a macro than a traditional function, because it does not immediately evaluate its first parameter (which should be a function call); instead, it causes that function call to run in a new COG. For example:
```
  static long stack[32];
  id = __builtin_cogstart(somefunc(a, b), &stack[0]);
```
runs `somefunc` with parameters `a` and `b` in a new COG, with a stack starting at `&stack[0]` (stacks grow up in FlexC).

The amount of space required for the stack depends on the complexity of the code to run, but must be at least 16 longs (64 bytes).

`__builtin_cogstart` returns the identifier of the new COG, or -1 if no COGs are free.

### EXPECT

Indicates the expected value for an expression. `__builtin_expect(x, y)` evaluates `x`, and indicates to the optimizer that the value will normally be `y`. This is provided for GCC compatibility, and the expected value is ignored by FlexC (so `__builtin_expect(x, y)` is treated the same as `(x)`).

### REV

```
x = __builtin_propeller_rev(y, n)
```
Reverses the bits of `y` and then shifts the result right by `32-n` places. This effectively means that the bottom `n` bits of `y` are reversed, and 0 placed in the remaining bits.

### SQRT

```
  x = __builtin_sqrt(y)
```
Calculates the square root of `y`. This is not like a normal C function in that the result type depends on the input type. If the input is an integer, the result is an integer. If the input is a float, the result is a float.

## propeller.h

Propeller 1 specific functions are contained in the header file `propeller.h`. Many of these work on P2 as well.

The propeller.h header file is not standardized. FlexC's library mostly follows the PropGCC propeller.h.

### cogstart

```
int cogstart(void (*func)(void *), void *arg, void *stack, size_tstacksize);
```
Starts the C function `func` with argument `arg` in another COG, using `stack` as its stack. Returns the COG started, or -1 on failure.

### getcnt

```
unsigned getcnt();
```
Fetches the current value of the CNT register. Using this instead of directly using CNT will make your code portable to P2.

### getpin

```
int getpin(int pin);
```
Returns the current state of input pin `pin`, either 0 or 1.

### setpin

```
int setpin(int pin, int val);
```
Sets the output pin `pin` to 0 if `val` is 0, or 1 otherwise.

### togglepin

```
void togglepin(int pin, int val);
```
Inverts the output pin `pin`.

## propeller2.h

Propeller 2 specific functions are contained in the header file `propeller2.h`. This file is usually quite portable among C compilers.

### COG and Clock control

#### _clkset

```
void _clkset(uint32_t clkmode, uint32_t clkfreq);
```
Sets the system clock to a new frequency `clkfreq`, using clock mode bits `clkmode`.  Note that correct setting of the clock depends on the actual crystal frequency of the hardware the program is being run on. No validation is performed by `_clkset`. The user is responsible for ensuring that `clkmode` is a valid mode and that `clkfreq` does in fact correspond to `clkmode` on this system.

#### _cnt

```
uint32_t _cnt(void);
```
Returns the low 32 bits of the system clock counter.

#### _cnth

```
uint32_t _cnth(void);
```
Returns the upper 32 bits of the system clock counter.

#### _cogchk

```
int _cogchk(int n);
```
Checks to see if cog `n` is running. Returns 1 if it is, 0 if it is not.

#### _coginit

```
int _coginit(int cogid, void *cogpgm, void *ptra)
```
Starts PASM code in another COG. `cogid` is the ID of the COG to start, or `ANY_COG` if a new one should be allocated. `cogpgm` points to the compiled PASM code to start, and `ptra` is a value to be placed in the new COG's `ptra` register. Returns the ID of the new COG, or -1 on failure.

#### _cogstop

```
void _cogstop(int cogid);
```
Stops the given COG.

#### _reboot

```
void _reboot(void);
```
Reboots the P2. Needless to say, this function never returns.

#### _waitx

```
void _waitx(uint32_t delay);
```
Waits for `delay` clock cycles.

### Regular Pin I/O

#### _pinf

```
void      _pinf(int pin);
```
Forces pin `pin` to float low.

#### _pinl

```
void      _pinl(int pin);
```
Makes pin `pin` an output and forces it low.

#### _pinh

```
void      _pinh(int pin);
```
Makes pin `pin` an output and forces it high.

#### _pinnot

```
void      _pinnot(int pin);
```
Makes pin `pin` an output and inverts it.

#### _pinrnd

```
void      _pinrnd(int pin);
```
Makes pin `pin` an output and sets it to a random bit value.

#### _pinr

```
int       _pinr(int pin);
```
Makes pin `pin` an input and returns its current value (0 or 1).

#### _pinw

```
void      _pinw(int pin, int val);
```
Makes pin `pin` an output and writes `val` to it. `val` should be only 0 or 1; results for other values are undefined.


## Disk I/O routines (P2 Only)

On the P2 there are some methods available for disk I/O. The `mount` call must be made before any other calls.

### Mount

The `mount` call gives a name to a file system. For example, after
```
mount("/host", _vfs_open_host());
mount("/sd", _vfs_open_sdcard());
```
files on the host PC may be accessed via names like "/host/foo.txt", "/host/bar/bar.txt", and so on, and files on the SD card may be accessed by names like "/sd/root.txt", "/sd/subdir/file.txt", and so on.

This only works on P2, because it requires a lot of HUB memory, and also needs the host file server built in to `loadp2`.

Available file systems are:

  * `_vfs_open_host()` (for the loadp2 Plan 9 file system)
  * `_vfs_open_sdcard()` for a FAT file system on the P2 SD card.

It is OK to make multiple mount calls, but they should have different names.

### Stdio

After mounting a file system, the standard FILE functions like `fopen`, `fprintf`, `fgets` and so on are available and usable.

### Posix file functions

`int remove(const char *path)`

Removes the regular file specified by `path`. Returns non-zero if the removal failed for some reason.

### Posix directory functions

A number of standard POSIX directory functions are available, including:

`int mkdir(const char *path)`

Creates a new directory named `path`. Returns 0 on success, non-zero on error (in which case errno is set to the specific error).

`int rmdir(const char *path)`

Removes the directory specified by `path`. Returns 0 on success, non-zero on failure (in the latter case sets `errno` to the precise error.

`int chdir(const char *path)`

Sets the current directory to `path`. Returns 0 on success, non-zero on failure.

`char *getcwd(char *buf, size_t size)`

Copies the current directory into `buf`, which must have at least `size` bytes available. Returns `buf`, or NULL if `buf` is not larget enough to hold the directory.

`DIR *opendir(const char *path)`

Opens `path` for reading with `readdir`. Returns NULL on error, otherwise a handle to use with `readdir`.

`int closedir(DIR *dir)`

Closes directory previously opened with `opendir`.

`struct dirent *readdir(DIR *dir)`

Reads the next directory entry.
