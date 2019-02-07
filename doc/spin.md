# Fastspin Spin

## Introduction

Fastspin was designed to accept the language Spin as documented in the Parallax Propeller Manual. It should be able to compile any Spin program, as long as there is space. The restriction is an important one; other Spin compilers produce Spin bytecode, a compact form that is interpreted by a program in the Propeller's ROM. Fastspin produces LMM code, which is basically a slightly modified Propeller machine code (slightly modified to run in HUB memory instead of COG). This is much larger than Spin bytecode, but also much, much faster.

## Preprocessor

fastspin has a pre-processor that understands basic directives like `#include`, `#define`, and`#ifdef / #ifndef / #else / #endif`.

### Directives

#### DEFINE
```
#define FOO hello
```
Defines a new macro `FOO` with the value `hello`. Whenever the symbol `FOO` appears in the text, the preprocessor will substitute `hello`.

Note that unlike the C preprocessor, this one cannot accept arguments. Only simple defines are permitted.

If no value is given, e.g.
```
#define BAR
```
then the symbol is defined as the string `1`.

#### IFDEF

Introduces a conditional compilation section, which is only compiled if
the symbol after the `#ifdef` is in fact defined.

#### IFNDEF

Introduces a conditional compilation section, which is only compiled if
the symbol after the `#ifndef` is *not* defined.

#### ELSE

Switches the meaning of conditional compilation.

#### ELSEIFDEF

A combination of `#else` and `#ifdef`.

#### ELSEIFNDEF

A combination of `#else` and `#ifndef`.

#### ERROR

Prints an error message. Mainly used in conditional compilation to report an unhandled condition. Everything after the `#error` directive is printed.

#### INCLUDE

Includes a file.

#### WARN

Prints a warning message.

#### UNDEF

Removes the definition of a symbol, e.g. to undefine `FOO` do:
```
#undef FOO
```

### Predefined Symbols

There are several predefined symbols:

Symbol           | When Defined
-----------------|-------------
`__propeller__`  | always defined to 1 (for P1) or 2 (for P2)
`__FASTSPIN__`   | if the `fastspin` front end is used
`__SPINCVT__`    | always defined
`__SPIN2PASM__`  | if --asm is given (PASM output) (always defined by fastspin)
`__SPIN2CPP__`   | if C++ or C is being output (never in fastspin)
`__cplusplus`    | if C++ is being output (never in fastspin)
`__P2__`         | if compiling for Propeller 2


## Extensions to Spin 1

Fastspin has a number of extensions to the Spin language.

### Absolute address

The `@@@` operator returns the absolute hub address of
a variable. This is the same as `@` in Spin code, but in PASM code `@`
returns only the address relative to the start of the `DAT` section.

### Conditional expressions

IF...THEN...ELSE expressions; you can use IF/THEN/ELSE in an expression, like:
```
r := if a then b else c
````
which is the same as
```
   if a then
     r := b
   else
     r := c
```
This may also be written in C / Verilog style as:
```
r := (a) ? b : c
```
In the latter form the parentheses around `a` are mandatory to avoid confusion with the random number operator `?`.

### Inline assembly

fastspin accepts inline assembly in `PUB` and `PRI` sections. Inline assembly starts with `asm` and ends with `endasm`. The inline assembly is still somewhat limited; the only operands permitted are immediate values, registers, local variables (including parameters and result values) of the containing function, or labels of that function. (Spin doesn't support goto and labels, but you can define labels in `asm` blocks and jump to them from other `asm` blocks that are in the same function. Some other languages supported by fastspin do have labels.)

Branching inside the function should work, but trying to return from it or to otherwise jump outside the function will almost certainly cause you to come to grief, even if the compiler allows it. Calling subroutines is also not permitted.

All non-branch instructions should work properly in inline assembly, as long as the operands satisfy the constraints above. Conditional execution is allowed, and flag bits may be set and tested.

If you need temporary variables inside some inline assembly, declare them as locals in the enclosing function.

Example:
```
PUB waitcnt2(newcnt, incr)
  asm
    waitcnt newcnt, incr
  endasm
  return newcnt
```
waits until CNT reaches "newcnt", and returns "newcnt + incr".

Note that unlike most Spin blocks, the `asm` block has to end with `endasm`. This is because indentation is not significant inside the assembly code. For example, labels typically start at the leftmost margin.

### Abstract objects and object pointers

The proposed Spin2 syntax for abstract object definitions and object pointers is accepted. A declaration like:
```
OBJ
  fds = "FullDuplexSerial"
```
declares `fds` as having the methods of a FullDuplexSerial object, but without any actual variable or storage being instantiated. Symbols declared this way may be used to cast parameters to an object type, for example:
```
PUB print(f, c)
  fds[f].dec(c)

PUB doprint22
  print(@aFullDuplexSerialObj, 22)
```

### Multiple return values and assignments

fastspin allows multiple return values and assignments. For example, to swap two variables `a` and `b` you can write:
```
  a,b := b,a
```
It's also acceptable (and perhaps easier to read) if there are parentheses around some or all of the multiple values:
```
  (a,b) := (b,a)
```

A function can also return multiple values. For instance, a function to calculate both a quotient and remainder could be written as:
```
PUB divrem(x,y)
  return x/y, x//y
```
or
```
PUB divrem(x,y) : q, r
  q := x/y
  r := x//y
```
This could later be used like:
```
  (a,digit) := divrem(a, 10)
```
It is also allowed to pass the multiple values returned from one function to
another. So for example:
```
' function to double a 64 bit number
PUB dbl64(ahi, alo): bhi, blo
  bhi := ahi
  blo := alo
  asm
    add blo, blo wc
    addx bhi, bhi
  endasm

' function to quadruple a 64 bit number
PUB quad64(ahi, alo)
  return dbl64(dbl64(ahi, alo))
```

### Array parameters

fastspin allows method parameters to be small arrays; in this case, the caller must supply one argument for each element of the array. For example:
```
pub selector(n, a[4])
  return a[n]

pub tryit
  return selector(n, 1, 2, 3, 4)
```
This particular example could be achieved via `lookup`, but there are other cases where it might be convenient to bundle parameters together in an array.

This feature is still incomplete, and may not work properly for C/C++ output.

### Default function parameters

fastspin permits function parameters to be given default values by adding `= X` after the parameter declaration, where `X` is a constant expression. For instance:
```
VAR
  long a

PUB inc(n=1)
  a += n

PUB main
  inc(2) ' adds 2 to a
  inc(1) ' adds 1 to a
  inc    ' same as inc(1)
  
```
The default values must, for now, be constant. Perhaps in the future this restriction will be relaxed, but there are some slightly tricky issues involving variable scope that must be resolved first.

#### Default string parameters

If a default function parameter is declared as a string, and a string literal is passed to it, that string literal is transformed into a string constant. Normally Spin uses just the first character of a string literal when one is seen in an expression (outside of STRING). Basically fastspin inserts a `string` operator around the literal in this case. So for example in:
```
PUB write(msg = string(""))
  '' do some stuff
...
  write(string("hello, world"))
  write("hello, world")
```
the two calls to `write` will do the same thing. In regular Spin, and in fastspin in the case where the default value is not present on a parameter, the second call will actually be interpreted as:
```
  write($68, $65, ..., 0)  ' $68 = ASCII value of "h"
```
which is probably not what was intended.

### Typed parameters and return values

The "expression" in a default parameter may also be a type name, for example `long`, `float`, or one of the pointer types `@long` (pointer to long), `@word` (pointer to word), `@byte`, or `@float`. These do not provide a default value, but do provide a hint to the compiler about what type of value is expected. This isn't terribly useful for Spin, but does make it possible for the compiler to check types and/or convert them if necessary for Spin functions called from C or BASIC.

Variables declared as return values may also be given type hints, which will be used to determine the type of the function.

Example:
```
' negate a floating point value
PUB negfloat(x = float) : r = float
  r := x ^ $80000000
```

### Spin2 operators

fastspin accepts some Spin2 operators:
```
  a \ b   uses the value of a, but then sets a to b
  x <=> y returns -1, 0, or 1 if x < y, x == y, or x > y
```

### Unsigned operators

fastspin has some new operators for treating values as unsigned
```
  a +/ b   is the unsigned quotient of a and b (treating both as unsigned)
  a +// b  is the unsigned remainder of a and b
  a +< b   is an unsigned version of <
  a +> b   is an unsigned version of >
  a +=< b  is an unsigned version of =<
  a +=> b  is an unsigned version of =>
```

### coginit/cognew

The `coginit` (and `cognew`) functions in Fastspin can start functions
from other objects than the current. (In "regular" Spin only functions
from the same object may be started this way.)

# Compatibility with other Spin compilers

## Limitations

There are very few Spin features that are not supported yet. `_FREE` and
`_STACK` are recognized, but do nothing.

There may be other features that do not work; if you find any,
please report them so they can be fixed.

The lexer and parser are different from the Parallax ones, so they may
well report errors on code the Parallax compiler accepts.

Timing of produced code is different, of course (in general much faster than
the native Spin interpreter). This may affect some objects; sometimes
developers left out delay loops in time critical code because the Spin
interpreter is so slow they weren't necessary. Watch out for this when porting
I2C, SPI and similar functions.

## Symbols

### Special characters in identifiers

Special characters like `$` may be included in Spin function and variable names
by preceding them with a backquote, e.g. to define a function `chr$` do:
```
  pub chr`$(x)
    return x
```

### Opcodes
In regular Spin opcodes like `TEST` are always reserved.

In fastspin, opcodes are only reserved inside `DAT` sections, so it is legal to have a function or variable named `test` or `add`.

### Reserved words

fastspin adds some reserved words: `asm`, `endasm`, and `then`.

## Array references

In regular Spin any symbol may be used as an array; that is, in
```
VAR
  long a, b
```
a reference to `a[1]` is the same as `b`. fastspin is stricter about this,
and only allows array references to symbols explicitly specified as arrays.

## Strings

Literal strings like `"hello"` are treated as lists of characters in regular Spin, unless enclosed in a `STRING` declaraction. So for example:
```
    foo("ABC")
```
is parsed as
```
    foo("A","B","C")
```
whereas in fastspin it is treated as an array of characters, so
it is parsed like:
```
    foo("ABC"[0])
```
which will be the same as
```
    foo("A")
```
The difference is rarely noticeable, because fastspin does convert string literals to lists in many places.
