Some of fastspin's optimizations
================================

Unused method removal
---------------------
This is pretty standard; if a method is not used, no code is emitted for it.

Multiplication conversion
-------------------------
Multiplies by powers of two, or numbers near a power of two, are converted to shifts. For example
```
    a := a*10
```
is converted to
```
    a := (a<<3) + (a<<1)
```

Dead code elimination
---------------------
Within functions if code can never be reached it is also removed. So for instance something like:
```
  CON
    pin = 1
  ... 
  if (pin == 2)
    foo
```
The if statement and call to `foo` are removed since the condition is always false.

Method inlining
---------------
Very small methods are expanded inline.

Also, if a method is called only once in a whole program, it is expanded inline at the call site.

Common Subexpression Elimination
--------------------------------
Code like something
```
   c := a*a + a*a
```
is automaticaly converted to something like:
```
    tmp := a*a
    c := tmp + tmp
```

Loop Strength Reduction
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
