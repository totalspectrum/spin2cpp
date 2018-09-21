# Fastspin BASIC

## WARNING:

BASIC support in fastspin is very much incomplete, and not usable for production
right now. Missing features include:

(1) Documentation; the manual below is just a start, much is incomplete.

(2) String support is very limited; strings may be assigned and printed, but that's about all for now. Still to be implemented are string concatenation and other string functions.

(3) There are some missing floating point functions, like sin and cos.

(4) Input and output isn't implemented yet, except for a basic PRINT statement. Eventually we will have re-directable PRINT, and a similar INPUT.

(5) There is no way to initialize an array.

(6) There are no functions yet for starting BASIC code on other COGs.

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
continue
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
from
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
shared
single
sqrt
step
sub
then
to
ubyte
uinteger
ulong
until
ushort
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
  function(x as integer) as string
  ...
```
### ASC

```
  i = ASC(s$)
```
returns the integer (ASCII) value of the first character of a string. If the
argument is not a string it is an error.

### ASM

Introduces inline assembly. This is not implemented yet.

### BYTE

A signed 8 bit integer, occupying one byte of computer memory. The unsigned version of this is `ubyte`. The difference arises with the treatment of the upper bit. Both `byte` and `ubyte` treat 0-127 the same, but for `byte` 128 to 255 are considered equivalent to -128 to -1 respectively (that is, when a `byte` is copied to a larger sized integer the upper bit is repeated into all the other bits; for `ubyte` the new bytes are filled with 0 instead).

### CASE

Used in a `select` statement. Not implemented yet.

### CLASS

A `class` is an abstract collection of variables and functions. If you've used the Spin language, a class is like a Spin object. In fact, Spin objects may be directly imported as classes:
```
   dim ser as class from "FullDuplexSerial.spin"
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
  class fds from "FullDuplexSerial.spin"
  ' create a variable of that type
  dim ser as fds
```
This is more convenient if there are many references to the class, or if you want to pass pointers to the class to functions.


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

### DECLARE

Keyword reserved for future use.

### DIM

Define variables and allocate memory for them. `dim` is the most common way to declare that variables exist. The simplest form just lists the variable names and (optionally) array sizes. The variable types are inferred from the names. For example, you can declare an array `a` of 10 integers, a single integer `b`, and a string `c$` with:
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

Repeat a loop while incrementing (or decrementing) a variable.

### FROM

Reserved.

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

### INTEGER

A 32 bit signed integer type. The unsigned 32 bit integer type is `uinteger`.

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

### NOT
```
  a = NOT b
```
Inverts all bits in the destination. This is basically the same as `b xor -1`.

In logical (boolean) conditions, since the TRUE condition is all 1 bits set, this operation has its usual effect of reversing TRUE and FALSE.

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

### OPEN

Reserved for future implementation.

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

### UBYTE

An unsigned 8 bit integer, occupying one byte of computer memory. The signed version of this is `byte`. The difference arises with the treatment of the upper bit. Both `byte` and `ubyte` treat 0-127 the same, but for `byte` 128 to 255 are considered equivalent to -128 to -1 respectively (that is, when a `byte` is copied to a larger sized integer the upper bit is repeated into all the other bits; for `ubyte` the new bytes are filled with 0 instead).

### VAR

Declare a local variable:
```
VAR i = 2
VAR msg$ = "hello"
```
`var` creates and initializes a new local variable (only available inside the function in which it is declared. The type of the new variable is inferred from the type of the expression used to initialize it; if for some reason that cannot be determined, the type is set according to the variable suffix (if any is present).

`var` is somewhat similar to `dim`, except that the type isn't given explicitly (it is determined by the initializer expression) and the variables created are always local, even if the `var` is in the main program (in the main program `dim` creates shared variables that may be used by functions or subroutines).

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

### XOR

```
  a = x xor y
```
Returns the bit-wise exclusive or of x and y. If x or y is a floating point number then it will be converted to integer before the operation is performed. `xor` is often used for flipping bits.

### Propeller Specific Functions

#### getcnt

```
  function getcnt() as uinteger
  x = getcnt()
```
Returns the current cycle counter. This is an unsigned 32 bit value that counts the number of system clocks elapsed since the device was turned on. It wraps after approximately 54 seconds.

#### waitcnt

Waits until the cycle counter is a specific value
```
  waitcnt(getcnt() + clkfreq) ' wait one second
```

#### waitpeq

Waits for pins to have a specific value

#### waitpne

Waits for pins to have a specific value

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
  output(pin) = output(pin) xor 1
  pausems 1000
loop

sub pausems(ms)
  waitcnt(getcnt() + ms * mscycles)
end sub
```
