Areas of incompatibility between fastspin and "standard" Spin
=============================================================

Symbols
-------

### Opcodes
In regular Spin opcodes like `TEST` are always reserved.

In fastspin, opcodes are only reserved inside `DAT` sections, so it is legal to have a function or variable named `test` or `add`.

### Reserved words

fastspin adds some reserved words: `asm`, `endasm`, and `then`.

Array references
----------------
In regular Spin any symbol may be used as an array; that is, in
```
VAR
  long a, b
```
a reference to `a[1]` is the same as `b`. fastspin is stricter about this,
and only allows array references to symbols explicitly specified as arrays.

Strings
-------

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
