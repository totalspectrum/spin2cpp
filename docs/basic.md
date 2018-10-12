# Fastspin BASIC

## WARNING:

BASIC support in fastspin is very much incomplete, and not usable for production
right now. Missing features include:

 - Documentation; the manual below is just a start, much is incomplete.

 - There are some missing floating point functions, like sin and cos.

 - Input and output isn't fully implemented yet.

 - There is no way to initialize an array.

 - There are only limited functions for starting BASIC code on other COGs.

## Introduction

Fastspin BASIC is the BASIC language support of the fastspin compiler for the Parallax Propeller and Prop2. It is a BASIC dialect similar to FreeBASIC or Microsoft BASIC, but with a few differences. On the Propeller chip it compiles to LMM code (machine language) which runs quite quickly.

fastspin recognizes the language in a file by the extension. If a file has a ".bas" extension it is assumed to be BASIC. Otherwise it is assumed to be Spin.

## Language Syntax

### Comments

Comments start with `rem` or a single quote character, and go to the end of line. Note that you need a space or non-alphabetical character after the `rem`; the word `remark` does not start a comment.

### Integers

Decimal integers are a sequence of digits, 0-9.

Hexadecimal (base 16) integers start with the sequence "&h", "0h", or "0x" followed by digits and/or the letters A-F or a-f. 

Binary (base 2) integers start with the sequence "&b" or "0b" followed by the digits 0 and 1.

Numbers may contain underscores anywhere to separate digits; those underscores are ignored.

For example, the following are all ways to represent the decimal number `10`:
```
   10
   1_0
   0xA
   &h_a
   &B1010
```

### Keywords

Keywords are always treated specially by the compiler, and no identifier may be
named the same as a keyword.
```
abs
and
any
as
asm
byte
case
class
close
continue
cpu
declare
dim
direction
do
double
else
end
endif
enum
exit
for
function
goto
if
input
integer
let
long
loop
mod
next
nil
not
open
or
output
pointer
print
program
ptr
put
rem
return
select
self
shared
short
single
sqrt
step
sub
then
to
type
ubyte
uinteger
ulong
until
ushort
using
var
wend
while
with
word
xor
```

### Variable, Subroutine, and Function names

Names of variables, subroutines, or functions ("identifiers") consist of a letter or underscore, followed by any sequence of letters, underscores, or digits. Names beginning with an underscore are reserved for system use. Case is ignored; thus the names `avar`, `aVar`, `AVar`, `AVAR`, etc. all refer to the same variable.

Identifiers may have a type specifier appended to them. `$` indicates a string variable or function, `%` an integer variable or function, and `#` a floating point variable or function. The type specifier is part of the name, so `a$` and `a#` are different identifiers. the first is a string variable. If no type specifier is appended, the identifier is assumed to be an integer.

Variable or function types may also be explicitly given, and in this case the type overrides any implicit type defined by the name. However, we strongly recommend that you not use type specifiers like `$` for variables (or functions) that you give an explicit type to.

Examples:

```
   dim a%            ' defines an integer variable
   dim a#            ' defines a different floating point variable
   dim a as string   ' defines another variable, this time a string
   dim a$ as integer ' NOT RECOMMENDED: overrides the $ suffix to make an integer variable

   '' this function returns a string and takes a float and string as parameters
   function f$(a#, b$)
     ...
   end function

   '' this function also returns a string from a float and string
   function g(a as single, b as string) as string
     ...
   end function
```

#### Arrays

Arrays must be declared with the `dim` keyword. Fastspin BASIC supports only
one dimensional arrays, but they may be of any type. Examples of array
declarations:
```
  rem an array of 10 integers
  dim a(10)
  rem same thing but more verbose
  dim b(10) as integer
  rem an array of 10 strings
  dim a$(10)
  rem another array of strings
  dim c(10) as string
```

Arrays are always indexed starting at 1. That is, if `a` is an array, then
`a(1)` is the first thing in the array, `a(2)` the second, and so on. This is
different from some other languages (such as Spin and C), where
array indexes start at 0. For example, a subroutine to initialize an array
to 0 could look like:
```
   dim a(10) as integer
   sub zero_a
     for i = 1 to 10
       a(i) = 0
     next i
   end sub
```

#### Variable Scope

There are two kinds of variables: global variables and local variables. Global variables may be accessed from anywhere in the program, and exist for the duration of the program. Local variables are only available inside the function or subroutine where they are declared, and only exist for as long as that function or subroutine is running. When the routine returns, the variables lose any values they had at the time. They are re-created afresh the next time the function is called.


## Language features

### Types

There are a number of data types built in to the Fastspin BASIC language.

#### Unsigned integer types

`ubyte`, `ushort`, and `uinteger` are the names for 8 bit, 16 bit, and 32 bit unsigned integers, respectively. The Propeller load instructions do not sign extend by default, so `ubyte` and `ushort` are the preferred names for 8 and 16 bit integers on the Propeller.

#### Signed integer types

`byte`, `short`, and `integer` are 8 bit, 16 bit, and 32 bit signed integers.

#### Floating point types

single is, by default, a 32 bit IEEE floating point number. There is an option to use a 16.16 fixed point number instead; this results in much faster calculations, but at the cost of much less precision and range.

#### Pointer types

Pointers to any other type may be declared.

#### String type

The `string` type is a special pointer. Functionally it is almost the same as a `const ubyte pointer`, but there is one big difference; comparisons involving a string compare the pointed to data, rather than the pointer itself. For example:
```
sub cmpstrings(a as string, b as string)
  if (a = b) then
    print "strings equal"
  else
    print "strings differ"
  end if
end sub

sub cmpptrs(a as const ubyte pointer, b as const ubyte pointer)
  if (a = b) then
    print "pointers equal"
  else
    print "pointers differ"
  end if
end sub

dim x as string
dim y as string

x = "hello"
y = "he" + "llo"
cmpstrings(x, y)
cmpptrs(x, y)
```
will always print "strings equal" followed by "pointers differ". That is because the `cmpstrings` function does a comparison with strings (so the contents are tested) but `cmppointers` does a pointer comparison (and while the pointers point at memory containing the same values, they are located in two distinct regions of memory and hence have different addresses.

### Function declarations

Function names follow the same rules as variable names. Like variable names, function names may have a type specifier appended, and the type specifier gives the type that the function returns.

```
function Add(a, b)
  return a+b
end function
```
This could be written more verbosely as
```
function Add(a as integer, b as integer) as integer
  return a+b
end function
```

### Memory allocation

Fastspin BASIC supports allocation of memory and garbage collection. Memory allocation is done from a small built-in heap. This heap defaults to 256 bytes in size, but this may be changed by defining a constant `HEAPSIZE` in the top level file of the program.

Garbage collection works by scanning memory for pointers that were returned from the memory allocation function. As long as references to the original pointers returned by functions like `left$` or `right$` exist, the memory will not be re-used for anything else.

Note that a CPU ("COG" in Spin terms) cannot scan the internal memory of other CPUs, so memory allocated by one CPU will only be garbage collected by that same CPU. This can lead to an out of memory situation even if in fact there is memory available to be claimed. For this reason we suggest that all allocation of temporary memory be done in one CPU only.

#### new and delete

The `new` operator may be used to allocate memory. `new` returns a pointer to enough memory to hold objects, or `nil` if not enough space is available for the allocation. For example, to allocate 40 bytes one can do:
```
  var ptr = new ubyte(40)
  if ptr then
    '' do stuff with the allocated memory
    ...
    '' now free it (this is optional)
    delete ptr
  else
    print "not enough memory"
  endif
```
The memory allocated by `new` is managed by the garbage collector, so it will ber reclaimed when all references to it have been removed. One may also explicitly free it with `delete`.

#### String functions

String functions and operators like `left$`, `right$`, and `+` (string concatenation) also work with allocated memory. If there is not enough memory to allocate for a string, these functions/operators will return `nil`.

#### Function pointers

Pointers to functions require 8 bytes of memory to be allocated at run time (to hold information about the object to be called). So for example in:
```
  '' create a Spin FullDuplexSerial object
  dim ser as class using "FullDuplexSerial.spin"
  '' get a pointer to its transmit function
  var tx = @ser.tx
```
the variable `tx` holds a pointer both to the `ser` object and to the particular method `tx` within it. Since this is dynamically allocated, it is possible for the `@` operator to fail and return `nil`.

## Propeller Specific Features

### Input, Output, and Direction

For the Propeller we have some special pseudo-variables `direction`, `input`, and `output`. These may be
used to directly control pins of the processor. For example, to set pin 1 as output and then set it high do:
```
  direction(1) = output
  output(1) = 1
```
Similarly, to set pin 2 as input and read it:
```
  direction(2) = input
  x = input(2)
```

#### Pin Ranges

Ranges of pins may be specified with `hi,lo` or `lo,hi`. The first form is preferred; if you do
```
  output(2, 0) = x
```
then the bottom 3 bits of x are copied directly to the first 3 output pins. If you use the other form
```
  output(0, 2) = x     ' note: x is reversed!
  output(0, 2) = &b110 ' sets bits 0 and 1 to 1, and bit 2 to 0
```
then the lower 3 bits are reversed; this is useful if you're directly coding a binary constant, but
otherwise is probably not what you want.

### Hardware registers

The builtin Propeller hardware registers are available with their usual names, unless they are redeclared. For example, the OUTA register is available as "outa" (or "OUTA", or "Outa"; case does not matter).

The hardware registers are not keywords, so they are not reserved to the system. Thus, it is possible to use `dim` to declare variables with the same name. Of course, if that is done then the original hardware register will not be accessible in the scope of the variable name.

## Alphabetical List of Keywords

### ABS

```
  y = abs x
```
Returns the absolute value of x. If x is a floating point number then so will be the result; if x is an unsigned number then it will be unchanged; otherwise the result will be an Integer.

### AND

```
  a = x and y
```
Returns the bit-wise AND of x and y. If x or y is a floating point number then it will be converted to integer before the operation is performed.

Also useful in boolean operations. The comparison operators return 0 for false conditions and all bits set for true conditions, so you can do things like:
```
  if (x < y AND x = z) then
    ' code that runs if both conditions are true
  end if
```

### ANY

```
  dim x as any
```
Declares x as a generic type 32 bit variable compatible with any other type. Basically this is a way to treat a variable as a raw 32 bit value. Note that no type checking at all is performed on variables declared with type `any`, nor are any conversions applied to them. This means that the compiler will not be able to catch many common errors.

`any` should be used only in exceptional circumstances.

Example: a subroutine to print the raw bit pattern of a floating point
number:
```
  sub printbits(x as single)
    dim a as any
    dim u as uinteger
    '' just plain u=x would convert x from single to unsigned
    '' instead go through an ANY type, which will do no conversion
    a = x
    u = a
    print u
  end sub
```

### AS

`as` is a keyword that introduces a type for a function, function parameter, or
dimensioned variable.

```
  ' declare a function with an integer parameter that returns a string
  function f(x as integer) as string
  ...
```
### ASC

```
  i = ASC(s$)
```
returns the integer (ASCII) value of the first character of a string. If the
argument is not a string it is an error.

### ASM

Introduces inline assembly. The block between ASM and END ASM is parsed slightly differently than usual; in particular, instruction names are treated as reserved identifiers.

Inside inline assembly any instructions may be used, but the only legal operands are integer constants and local variables (or parameters) to the function which contains the inline assembly. Labels may be defined, and may be used as the target for `goto` elsewhere in the function.

Example: to implement a wait (like the built-in `waitcnt`:
```
sub wait_until_cycle(x as uinteger)
  asm
    waitcnt x
  end asm
end sub
```

### BYTE

A signed 8 bit integer, occupying one byte of computer memory. The unsigned version of this is `ubyte`. The difference arises with the treatment of the upper bit. Both `byte` and `ubyte` treat 0-127 the same, but for `byte` 128 to 255 are considered equivalent to -128 to -1 respectively (that is, when a `byte` is copied to a larger sized integer the upper bit is repeated into all the other bits; for `ubyte` the new bytes are filled with 0 instead).

### CASE

Used in a `select` statement. Not implemented yet.

### CHR$

Not actually a reserved word, but a built-in function. Converts an ascii
value to a string (so the reverse of ASC). For example:
```
print chr$(65)
```
prints `A` (the character whose ASCII value is 65)

### CLASS

A `class` is an abstract collection of variables and functions. If you've used the Spin language, a class is like a Spin object. In fact, Spin objects may be directly imported as classes:
```
   dim ser as class using "FullDuplexSerial.spin"
```
creates an object `ser` based on the Spin file "FullDuplexSerial.spin"; this is the same as the Spin declaration:
```
   OBJ ser: "FullDuplexSerial"
```
BASIC files may also be used as classes. When they are, all the functions and subroutines in the BASIC file are exposed as methods (there are no private methods in BASIC yet). Any BASIC code that is not in a function or subroutine is not accessible.

#### Abstract classes

Another way to define an object is to first declare an abstract `class` with a name, and then use that name in the `dim` statement:
```
  ' create abstract class fds representing Spin FullDuplexSerial
  class fds using "FullDuplexSerial.spin"
  ' create a variable of that type
  dim ser as fds
```
This is more convenient if there are many references to the class, or if you want to pass pointers to the class to functions.

#### Class Example in BASIC

`Counter.bas` (the class) contains:
```
dim x as integer

sub reset
  x = 0
end sub

sub inc(n = 1)
  x = x + n
end sub

function get()
  return x
end function
```

`Main.bas` (the main program) contains:
```
  dim cnt as class using "Counter.bas"

  cnt.reset
  cnt.inc
  cnt.inc
  print cnt.get() ' prints 2
```
This is compiled with:
```
  fastspin main.bas
```

### CLOSE

Closes a file previously opened by `open`. This causes the `closef` function specified in the device driver (if any) to be called, and then invalidates the handle so that it may not be used for further I/O operations. Any attempt to use a closed handle produces no result.
```
  close #2  ' close handle #2
```
Note that handles 0 and 1 are reserved by the system; closing them may produce undefined results.

### CONST

At the beginning of a line, CONST declares a constant value. For example:
```
  const x = 1
```
declares x to be the integer 1. Only integer constants may be declared this way.

Inside a type name, CONST signifies that variables of this type may not be modified. This is mainly useful for indicating that pointers should be treated as read-only.
```
   sub trychange(s as const ubyte ptr)
     s(1) = 0  '' illegal, s points to const ubytes
     if (s(1) = 2) then '' OK, s may be read
       print "it was 2"
     end if
   end sub
```

### CONTINUE

Used to resume loop execution early. Not implemented yet.

### CPU

Used to start a subroutine running on another CPU. The parameters are the subroutine call to execute, and a stack for the other CPU to use. For example:
```
' blink a pin at a given frequency
sub blink(pin, freq)
  direction(pin) = output
  do
    output(pin) = not output(pin)
    waitcnt(getcnt() + freq)
  loop
end sub
...
dim stack(8) ' small stack, blink does not call many other functions

' start the blinking up on another CPU
var a = cpu(blink(LED, 80_000_000), @stack(1))
```
`cpu` returns the cpu id ("cog id") of the CPU that the new function is running on.

### DECLARE

Keyword reserved for future use.

### DELETE

Free memory allocated by `new` or by one of the string functions (`+`, `left$`, `right$`, etc.).

Use of `delete` is a nice hint and makes sure the memory is free, but it is not strictly necessary since the memory is garbage collected automatically.

### DIM

Dimension variables. This defines variables and allocate memory for them. `dim` is the most common way to declare that variables exist. The simplest form just lists the variable names and (optionally) array sizes. The variable types are inferred from the names. For example, you can declare an array `a` of 10 integers, a single integer `b`, and a string `c$` with:
```
dim a(10), b, c$
```
It's also possible to give explicit types with `as`:
```
dim a(10) as integer, b as ubyte, s as string
```
If you give an explicit type, it will apply to all the previous variables without an explicit type:
```
' this makes all the variables singles, despite their names
dim a(10), b%, c$, d as single
```
It's probably better (clearer) in this case to put the `as` first:
```
dim as single a(10), b%, c$, d
```

Variables declared inside a function or subroutine are "local" to that function or subroutine, and are not available outside or to other functions or subroutines. Variables dimensioned at the top level may be used by all functions and subroutines in the file.

See also VAR.

### DIRECTION

Pseudo-array of bits describing the direction (input or output) of pins. In Propeller 1 this array is 32 bits long, in Propeller 2 it is 64 bits.

```
  direction(2) = input ' set pin 2 as input
  direction(6,4) = output ' set pins 6, 5, 4 as outputs
```
Note that pin ranges may not cross a 32 bit boundary; that is,
```
  direction(33, 30) = input
```
is illegal and produces undefined behavior.

### DO

Main loop construct. A `do` loop may have the loop test either at the beginning or end, and it may run the loop while a condition is true or until a condition is true. For example:
```
  do
    x = input(9)
  loop until x = 0
```
will wait until pin 9 is 0.

See also WHILE.

### DOUBLE

The type for a double precision floating point number. `double` is not actually implemented in the compiler, and is treated the same as `single`.

### ELSE

See IF

### END

Used to mark the end of most blocks. For example, `end function` marks the end of a function declaration, and `end if` the end of a multi-line `if` statement. In most cases the name after the `end` is optional.

### ENDIF

Marks the end of a multi-line `if` statement. Same as `end if`.

### ENUM

Reserved for future use.

### EXIT

Exit early from a `for`, `do`, or `while` loop. Not implemented yet.

### FOR

Repeat a loop while incrementing (or decrementing) a variable. The default step value is 1, but if an explicit `step` is given this is used instead:
```
' print 1 to 10
for i = 1 to 10
  print i
next i
' print 1, 3, 5, ..., 9
for i = 1 to 10 step 2
  print i
next i
```

If the variable given in the loop is not already defined, it is created as a variable shared by all functions in the module. This tends to be inefficient, so it is better to explicitly dimension the variable as a local variable rather than letting `for` create it as a shared.

### FUNCTION

Defines a new function. The type of the function may be given explicitly with an `as` _type_ clause; if no such clause exists the function's type is deduced from its name. For example, a function whose name ends in `$` is assumed to return a string unless an `as` is given.

Functions have a fixed number and type of arguments, but the last arguments may be given default values with an initializer. For example,
```
  function inc(n as integer, delta = 1 as integer) as integer
    return n + delta
  end function
```
defines a function which adds two integers and returns an integer result. Since the default type of variables is integer, this could also be written as:
```
  function inc(n, delta = 1)
    return n+delta
  end function
```
In this case because the final argument `delta` is given a default value of 1, callers may omit this argument. That is, a call `inc(x)` is exactly equivalent to `inc(x, 1)`.

#### GETCNT

Propeller specific builtin function.

```
  function getcnt() as uinteger
  x = getcnt()
```
Returns the current cycle counter. This is an unsigned 32 bit value that counts the number of system clocks elapsed since the device was turned on. It wraps after approximately 54 seconds.

### GOTO

`goto x` jumps to a label `x`, which must be defined in the same function.
Labels are defined by giving an identifier followed by a `:`. For example:
```
  if (x=y) goto xyequal
  print "x differs from y"
  goto done
xyequal:
  print "x and y are equal"
done:
```
Note that in most cases code written with a `goto` could better be written with
`if` or `do` (for instance the example above would be easier to read if written with `if` ... `then` ... `else`). `goto` should be used sparingly.

### HEAPSIZE
```
  const HEAPSIZE = 256
```
Declares the amount of space to be used for internal memory allocation by things like string functions. The default is 256 bytes, but if your program does a lot of string manipulation and/or needs to hold on to the allocations for a long time, you may need to increase this by explicitly declaring `const HEAPSIZE` with a larger value.

### IF

An IF statement introduces some code that should be executed only if a
condition is true:
```
if x = y then
  print "x and y are the same"
else
  print "x and y are different"
end if
```

There are two forms of `if`. A "simple if" occupies just one line, executes one statement if the condition is true, and has no `else` clause. Simple ifs do not have a `then`:
```
' simple if example
if x = y print "they are equal"
```

Regular or compound if statements have a `then` and continue on until the next matching `else` or `end if`. If you want to have an `else` condition then you will have to use this form of if:
```
if x = y then
  print "they are equal"
else
  print "they differ"
end if
```
You may also put an if statement after an else:
```
if x = y then
  print "x and y are the same"
  print "I don't know about z"
else if x = z
  print "x and z are the same, and different from y"
else
  print "x does not equal either of the others"
end if
```

### INPUT

A pseudo-array of bits representing the state of input bits. On the Propeller 1 this is the 32 bit INA register, but on Propeller 2 it is 64 bits.

Bits in `input` may be read with an array-like syntax:
```
   x = input(0)    ' read pin 0
   y = input(4,2)  ' read pins 4,3,2
```
Note that usually you will want to read the pins with the larger pin number first, as the bits are labelled with bit 31 at the high bit and bit 0 as the low bit.

Also note that before using a pin as input its direction should be set as input somewhere in the program:
```
   direction(4,0) = input  ' set pins 4-0 as inputs
```

### INPUT$

A predefined string function. `input$(n, h)` reads `n` characters from handle `h`, as created by an `open device as #h` statement.

### INTEGER

A 32 bit signed integer type. The unsigned 32 bit integer type is `uinteger`.

### LEFT$

A predefined string function. `left$(s, n)` returns the left-most `n` characters of `s`. If `n` is longer than the length of `s`, returns `s`. If `n` =< 0, returns an empty string. If a memory allocation error occurs, returns `nil`.

### LET

Variable assignment:
```
  let a = b
```
sets `a` to be equal to `b`. This can usually be written as:
```
  a = b
```
the only difference is that in the `let` form if `a` does not already exist it is created as a global variable (one accessible in all functions). The `let` keyword is deprecated in some versions of BASIC (such as FreeBASIC) so it's probably better to use `var` or `dim` to explicitly declare your variables.

### LONG

A signed 32 bit integer. An alias for `integer`. The unsigned version of this is `ulong`.

### LOOP

Marks the end of a loop introduced by `do`. See DO for details.

### MOD

`x mod y` finds the integer remainder when `x` is divided by `y`.

Note that if both the quotient and remainder are desired, it is best to put the calculations close together; that way the compiler may be able to combine the two operations into one (since the software division code produces both quotient and remainder). For example:
```
  q = x / y
  r = x mod y
```

### NEW

Allocates memory from the heap for a new object, and returns a pointer to it. May also be used to allocate arrays of objects. The name of the type of the new object appears after the `new`, optionally followed by an array size:
```
  var x = new ubyte(10)  ' allocate 10 bytes and return a pointer to it
  x(1) = 1               ' set a variable in it
  
  class FDS using "FullDuplexSerial.spin"
  var ser = new FDS      ' allocate space for a new full duplex serial object
  ser.start(31, 30, 0, 115_200) ' start up the new object
```
See the discussion of memory allocation for tips on using `new`. Note that the default heap is rather small, so you will probably need to declare a larger `HEAPSIZE` if you use `new` a lot.

Memory allocated by `new` may be explicitly freed with `delete`; or, it may left to be garbage collected automatically.

### NEXT

Indicates the end of a `for` loop. The variable used in the loop may be placed after the `next` keyword, but this is not mandatory. If a variable is present though then it must match the loop.

See FOR.

### NIL

A special pointer value that indicates an invalid pointer. `nil` may be returned from any string function or other function that allocates memory if there is not enough space to fulfil the request. `nil` is of type `any` and may be assigned to any variable. When assigned to a numeric variable it will cause the variable to become 0.

### NOT
```
  a = NOT b
```
Inverts all bits in the destination. This is basically the same as `b xor -1`.

In logical (boolean) conditions, since the TRUE condition is all 1 bits set, this operation has its usual effect of reversing TRUE and FALSE.

### OPEN

Open a handle for input and/or output. The general form is:
```
  open device as #n
```
where `device` is a device driver structure returned by a system function such as `SendRecvDevice`, and `n` evaluates to an integer between 2 and 7. (Handles 0 and 1 also exist, but are reserved for system use.)

Example:
```
  open SendRecvDevice(@ser.tx, @ser.rx, @ser.stop) as #2
```
Here the `SendRecvDevice` is given pointers to functions to call to send a single character, to receive a single character, and to be called when the handle is closed. Any of these may be `nil`, in which case the corresponding function (output, input, or close) does nothing.

### OR

```
  a = x or y
```
Returns the bit-wise inclusive OR of x and y. If x or y is a floating point number then it will be converted to integer before the operation is performed.

Also useful in boolean operations. The comparison operators return 0 for false conditions and all bits set for true conditions, so you can do things like:
```
  if (x < y OR x = z) then
    ' code that runs if either condition is true
  end if
```

### OUTPUT

A pseudo-array of bits representing the state of output bits. On the Propeller 1 this is the 32 bit OUTA register, but on Propeller 2 it is 64 bits.

Bits in `output` may be read and written an array-like syntax:
```
   output(0) = not output(0)   ' toggle pin 0
   output(4,2) = 1  ' set pins 4 and 3 to 0 and pin 2 to 1
```
Note that usually you will want to access the pins with the larger pin number first, as the bits are labelled with bit 31 at the high bit and bit 0 as the low bit.

Also note that before using a pin as output its direction should be set as output somewhere in the program:
```
   direction(4,0) = output  ' set pins 4-0 as outputs
```

### PAUSEMS


A built-in subroutine to pause for a number of milliseconds. For example, to pause for 2 seconds, do
```
  pausems 2000
```

### PRINT

`print` is a special subroutine that prints data to a serial port or other stream. The default destination for `print` is the pin 30 (pin 62 on P2) serial port, running at 115_200 baud.

More than one item may appear in a print statement. If items are separated by commas, a tab character is printed between them. If they are separated by semicolons, nothing is printed between them, not even a space; this differs from some other BASICs.

If the print statement ends in a comma, a tab is printed at the end. If it ends in a semicolon, nothing is printed at the end. Otherwise, a newline (carriage return plus line feed) is printed.

As a special case, if a backslash character `\` appears in front of an expression, the value of that expression is printed as a single byte character.

Examples
```
   ' basic one item print
   print "hello, world!"
   ' two items separated by a tab
   print "hello", "world!"
   ' two items with no separator
   print "hello"; "world"
   ' an integer, with no newline
   print 1;
   ' a string and then an integer, nothing between them
   print "then "; 2
```
prints
```
hello, world!
hello  world
helloworld
1then 2
```

`print` may be redirected. For example,
```
print #2, "hello, world"
```
prints its message to the device previously `open`ed as device #2.

### PRINT USING

Formats output using a string. The general form of this is:
```
  print using STRING; expr [,expr...] [;]
```
where `STRING` is a string literal and `expr` is one or more expressions.

Within the string literal output fields are specified by special forms, which are replaced by the various expressions.

`&` indicates a variable width field, within which the numbers or strings are printed with the minimum number of characters.

`#` starts a numeric field with space padding; the number of `#` characters indicates the width of the field. The numeric value is printed right-justified within the field. If it cannot fit, the first digit which will fit is replaced with '#' and the rest are printed normally. If the field is preceded by a `-` or `+` the sign is printed there; otherwise, if the value is negative then the `-` sign is included in the digits to print.

`%` starts a numeric field with 0 padding; the number of `%` characters indicates the width of the field. Leading zeros are explicitly printed. If the number cannot fit in the indicated number of digits, the first digit which will fit is replaced with '#' and the rest are printed normally.

`+` indicates that a place should be reserved for a sign character (`+` for non-negative, `-` for negative). `+` must immediately be followed by a numeric field. If the argument is an unsigned integer, instead of `+` a space is always printed.

`-` indicates that a place should be reserved for a sign character (space for non-negative, `-` for negative). `-` must immediately be followed by a numeric field. If the argument is an unsigned integer, a space is always printed.

`!` indicates to print a single character (the first character of the string argument).

`\` indicates a string field, which continues until the next `\`. The width of the field is the total number of characters, including the beginning and ending `\`. The string will be printed left justified within the field. Centering or right justification may be achieved for fields of length 3 or more by using `=` or '>' characters, respectively, as fillers between `\`. If the string is too long to fit within the field, only the first `N` characters of the string are printed.


```
print using "%%%%"; x
```

### PROGRAM

This keyword is reserved for future use.

The statements in the top level of the file (not inside any subroutine or function) are placed in a method called `program`. This is only really useful for calling them from another language (for example a Spin program using a BASIC program as an object).

### REM

Introduces a comment, which continues until the end of the line. A single quote character `'` may also be used for this.

### RETURN

Return from a subroutine or function. If this statement occurs inside a function, then the `return` keyword must be followed by an expression giving the value to return; this expression should have a type compatible with the function's return value.

### RIGHT$

A predefined string function. `right$(s, n)` returns the right-most `n` characters of `s`. If `n` is longer than the length of `s`, returns `s`. If `n` =< 0, returns an empty string. If a memory allocation error occurs, returns `nil`.

### SELF

Indicates the current object. Not implemented yet.

### SENDRECVDEVICE

A built-in function rather than a keyword. `SendRecvDevice(sendf, recvf, closef)` constructs a simple device driver based on three functions: `sendf` to send a single byte, `recvf` to receive a byte (or return -1 if no byte is available), and `closef` to be called when the device is closed. The value(s) returned by `SendRecvDevice` is only useful for passing directly to the `open` statement, and should not be used in any other context (at least not at this time).

### SHORT

A signed 16 bit integer, occupying two bytes of computer memory. The unsigned version of this is `ushort`. The difference arises with the treatment of the upper bit. Both `short` and `ushort` treat 0-32767 the same, but for `short` 32768 to 65535 are considered equivalent to -32768 to -1 respectively (that is, when a `short` is copied to a larger sized integer the upper bit is repeated into all the other bits; for `ushort` the new bits are filled with 0 instead).

### SINGLE

Single precision floating point data type. By default this is an IEEE 32 bit single precision float, but compiler options may change this (for example to a 16.16 fixed point number).

### STEP

Gives the increment to apply in a FOR loop.
```
for i = 2 to 8 step 2
  print i
next
```
will print 2, 4, 6, and 8 on separate lines.

### THEN

Introduces a multi-line series of statements for an `if` statement. See IF for details.

### TO

A syntactical element typically used for giving ranges of items.

### TYPE

Creates an alias for a type. For example,
```
  type uptr as ubyte ptr
```
creates a new type name `uptr` which is a pointer to a `ubyte`. You may use the new type name anywhere a type is required.

### UBYTE

An unsigned 8 bit integer, occupying one byte of computer memory. The signed version of this is `byte`. The difference arises with the treatment of the upper bit. Both `byte` and `ubyte` treat 0-127 the same, but for `byte` 128 to 255 are considered equivalent to -128 to -1 respectively (that is, when a `byte` is copied to a larger sized integer the upper bit is repeated into all the other bits; for `ubyte` the new bytes are filled with 0 instead).

### UINTEGER

An unsigned 32 bit integer.

### ULONG

An unsigned 32 bit integer, occupying four bytes of computer memory. The signed version of this is `long`.

### USHORT

An unsigned 16 bit integer, occupying two bytes of computer memory. The signed version of this is `short`. The difference arises with the treatment of the upper bit. Both `short` and `ushort` treat 0-32767 the same, but for `short` 32768 to 65535 are considered equivalent to -32768 to -1 respectively (that is, when a `short` is copied to a larger sized integer the upper bit is repeated into all the other bits; for `ushort` the new bits are filled with 0 instead).

### USING

Keyword intended for use in PRINT statements, and also to indicate the file to be used for a CLASS.

### VAR

Declare a local variable:
```
VAR i = 2
VAR msg$ = "hello"
```
`var` creates and initializes a new local variable (only available inside the function in which it is declared. The type of the new variable is inferred from the type of the expression used to initialize it; if for some reason that cannot be determined, the type is set according to the variable suffix (if any is present).

`var` is somewhat similar to `dim`, except that the type isn't given explicitly (it is determined by the initializer expression) and the variables created are always local, even if the `var` is in the main program (in the main program `dim` creates shared variables that may be used by functions or subroutines).

### WAITCNT

Propeller specific builtin function. Waits until the cycle counter is a specific value
```
  waitcnt(getcnt() + clkfreq) ' wait one second
```

#### WAITPEQ

Propeller specific builtin function. Waits for pins to have a specific value (given by a bit mask). Same as the Spin `waitpeq` routine. Note that the arguments are bit masks, not pin numbers, so take care when porting code from PropBasic.

#### WAITPNE

Propeller specific builtin function. Waits for pins to not have a specific value (given by a bit mask). Same as the Spin `waitpne` routine. Note that the arguments are bit masks, not pin numbers, so take care when porting code from PropBasic.


### WHILE

Begins a loop which continues as long as a specified condition is true.
```
  ' wait for pin to go low
  loopcount = 0
  while input(1) <> 0
    loopcount = loopcount + 1
  wend
  print "waited "; loopcount; " times until pin went high"
```
The end of the repeated code may be terminated either with `wend` or with `end while`.

The `while` loop may also be written as `do while`:
```
  do while input(1) <> 0
    loopcount = loopcount + 1
  loop
```
or
```
  do until input(1) = 0
    loopcount = loopcount + 1
  loop
```

### WORD

An alias for `ushort`, a 16 bit unsigned word.

### XOR

```
  a = x xor y
```
Returns the bit-wise exclusive or of x and y. If x or y is a floating point number then it will be converted to integer before the operation is performed. `xor` is often used for flipping bits.

### Propeller Specific Variables

#### clkfreq

```
  dim clkfreq as uinteger
```
Clkfreq gives the frequency of the system clock in cycles per second.

## Sample Programs

### Toggle a pin

This program toggles a pin once per second.
```
rem simple program to toggle a pin

const pin = 16

let mscycles = clkfreq / 1000

direction(pin) = output

do
  output(pin) = not output(pin)
  pausems 1000
loop
```
