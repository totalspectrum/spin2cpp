Some of fastspin's optimizations
================================

Below are discussed some of the optimizations performed by fastspin, and at what level they are enabled.

Multiplication conversion (always)
-------------------------
Multiplies by powers of two, or numbers near a power of two, are converted to shifts. For example
```
    a := a*10
```
is converted to
```
    a := (a<<3) + (a<<1)
```

A similar optimization is performed for divisions by powers of two.

Unused method removal (-O1)
---------------------
This is pretty standard; if a method is not used, no code is emitted for it.

Dead code elimination (-O1)
---------------------
Within functions if code can obviously never be reached it is also removed. So for instance in something like:
```
  CON
    pin = 1
  ... 
  if (pin == 2)
    foo
```
The if statement and call to `foo` are removed since the condition is always false.

Small Method inlining (-O1)
---------------------
Very small methods are expanded inline.

Register optimization (-O1)
---------------------
The compiler analyzes assignments to registers and attempts to minimize the number of moves (and temporary registers) required.

Branch elimination (-O1)
------------------
Short branch sequences are converted to conditional execution where possible.

Constant propagation (-O1)
--------------------
If a register is known to contain a constant, arithmetic on that register can often be replaced with move of another constant.

Peephole optimization (-O1)
---------------------
In generated assembly code, various shorter combinations of instructions can sometimes be substituted for longer combinations.

Loop optimization (basic in -O1, stronger in -O2)
-----------------
In some circumstances the optimizer can re-arrange counting loops so that the `djnz` instruction may be used instead of a combination of add/sub, compare, and branch. In -O2 a more thorough loop analysis makes this possible in more cases.

Fcache (-O1 for P1, -O2 for P2)
------
Small loops are copied to internal memory (COG on P1, LUT on P2) to be executed there. These loops cannot have any non-inlined calls in them.

Single Use Method inlining (-O2)
--------------------------
If a method is called only once in a whole program, it is expanded inline at the call site.

Common Subexpression Elimination (-O2)
--------------------------------
Code like:
```
   c := a*a + a*a
```
is automaticaly converted to something like:
```
    tmp := a*a
    c := tmp + tmp
```

Loop Strength Reduction (-O2)
-----------------------

### Array indexes

Array lookups inside loops are converted to pointers. So:
```
    repeat i from 0 to n-1
       a[i] := b[i]
```
is converted to the equivalent of
```
    aptr := @a[0]
    bptr := @b[0]
    repeat n
      long[aptr] := long[bptr]
      aptr += 4
      bptr += 4
```

### Multiply to addition

An expression like `(i*100)` where `i` is a loop index can be converted to
something like `itmp \ itmp + 100`
