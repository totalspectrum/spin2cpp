# PASM Output

This document describes some details of the PASM code output by flexprop.

Calling Conventions
===================

There are two ways that functions can be called: "fast call" and "stack call".

Fast Call
---------

In "fast call" arguments are passed in registers `arg1`, `arg2`, `arg3`, and so
on, and the result comes back in `result1` (with a secondary result in `result2`
if applicable).

Local variables in a fast call function are kept in registers as well. These
are unique to each function.

The return address is handled specially depending on the platform. For code
in cog memory, it is placed as if by a "call" instruction (either in
the "ret" instruction itself, for P1, or on the internal stack for P2).
For code in hub memory, it is pushed on the stack.

Stack Call
----------

In "stack call" arguments are passed on the stack, laid out in the same way
that the Spin interpreter does. On entry the stack will look like:

```
sp - 4: return address
sp:     result space (initialized to 0 by caller)
sp + 4: first argument
sp + 8: second argument
...
sp + n: last argument
```

Stack call functions may have only one result value.

On entry a stack call function saves the stack pointer into the `fp`
(frame pointer) register, then increments the stack to skip over the
parameters and space for local variables.

In a stack call function the local variables and parameters are kept on the
stack. Some scratch registers may be allocated in cog memory for working.

If a fast call function is called recursively, then on entry it pushes the
current values of its local and working variables onto the stack, then
pops them off at exit.

Choice of calling convention
----------------------------

The default calling convention is "fast call". "Stack call" is used for
a method in the following circumstances:

(a) if the @ operator is applied to a local variable or parameter (except
    that if the @ appears only in a longmove with a constant count < 4,
    the compiler can unroll that longmove and continue to use fast call)
(b) if it is the target of a coginit or cognew

Calling from Cog To Hub
-----------------------

Calling from hub to cog is straightforward -- it's just like a cog to cog
call, since the hub instruction is being interpreted in cog memory in
the LMM kernel.

Going the other way is slightly trickier; we need a way to return to
cog code from hub. The solution is a small hub stub that jumps back
into cog.  This allows us to have one level of call from hub to
cog. However, it will break down if we call from hub to cog then back
to hub and then back to cog again. At the moment there's not actually
any way to place just one function into cog memory, so this can't
happen yet in practice -- the only cog to hub calls are generated
internally and can avoid this. But it's something to watch out for in
the future.
