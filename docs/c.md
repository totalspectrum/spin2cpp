# Flex C (the C dialect accepted by fastspin)

## DEVELOPMENT STATUS

C compiler support is not even at the "beta" stage yet; there are many features missing.

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

The inline assembly syntax is similar to that of MSVC. Inline assembly blocks are marked with the keyword `asm`. For example, a function to get the current cog id could be written as:
```
int getcogid() {
   int x;
   asm {
      cogid x
   }
   return x;
}
```
The `asm` keyword must be followed by a `{`; everything between that and the next `}` is taken to be assembly code.

Inside inline assembly any instructions may be used, but the only legal operands are integer constants and local variables (or parameters) to the function which contains the inline assembly. Labels may be defined, and may be used as the target for `goto` elsewhere in the function.

