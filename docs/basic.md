# Fastspin BASIC

## WARNING:

BASIC support in fastspin is very much incomplete, and not usable for production
right now. Missing features include:

(1) Documentation; the manual below is just a start, much is incomplete.

(2) String support is very limited; strings may be assigned and printed, but that's about all for now. Still to be implemented are string comparisons, concatenation, and other string functions.

(3) Floating point support is incomplete. The "single" datatype is currently represented by 16.16 fixed point numbers. These are fast, but the very limited range is a problem. Eventually we will have proper IEEE floating point, with the fixed point kept as an option.

(4) Parameter passing is incomplete; parameters are not properly promoted (e.g. an integer passed to a parameter of type single will get the wrong bit pattern).

(5) Function return type aren't checked, and you can't give a non-default return type for a function.

(6) Input and output isn't implemented yet, except for a basic PRINT statement. Eventually we will have re-directable PRINT, and a similar INPUT.

(7) There are no functions yet for starting BASIC code on other COGs.

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
   0hA
   &h_a
   &B1010
```

### Keywords

Keywords are always treated specially by the compiler, and no identifier may be
named the same as a keyword. The keyword `rem` is special, in that those 3 characters start a comment, so no identifier may start with `rem`.
```
abs
and
any
as
asm
byte
class
continue
declare
dim
direction
do
double
else
end
enum
exit
for
function
goto
if
input
let
long
loop
mod
next
or
output
pointer
print
program
rem
return
shared
single
sqrt
step
sub
then
to
until
wend
while
with
word
```

### Variable, Subroutine, and Function names

Names of variables, subroutines, or functions ("identifiers") consist of a letter, followed by any sequence of letters or digits. Underscores may be inserted anywhere in this sequence, but are ignored for purposes of comparing variables. Similarly, case is ignored. So the names `avar`, `a_var`, `AVar`, `A_VAR`, etc. all refer to the same variable.

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

##### Pin Ranges

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

### Propeller Specific Functions

#### getcnt

```
  function getcnt() as uinteger
  x = getcnt()
```
Returns the current cycle counter. This is an unsigned 32 bit value that counts the number of system clocks elapsed since the device was turned on. It wraps after approximately 54 seconds.

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
