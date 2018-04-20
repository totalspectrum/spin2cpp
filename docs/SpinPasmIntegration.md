Automatically Converting Spin To Pasm
=====================================

Introduction
------------

Spin is the standard programming language for the Propeller. It's
generally implemented by means of a bytecode compiler on the PC,
producing binaries that are interpreted by a bytecode interpreter in
the Propeller ROM. This works well -- Spin programs are compact, and
perform reasonably well. But sometimes you need more performance than
an interpreter can give.

The fastspin compiler can convert Spin programs to PASM
automatically. Originally fastspin only worked on whole programs,
which was fine if you wanted to speed up a small program. But PASM
code is a lot bigger then bytecode, so converting a whole program to
PASM increases its size quite a lot, and may not be feasible for
larger programs.

The latest release of fastspin (3.7.0) allows individual Spin objects
to be converted to PASM and then integrated with regular Spin programs
(compiled with the Propeller Tool, openspin, or other Spin bytecode
compilers).

Installing Fastspin
-------------------

This is straightforward. Just download fastspin.zip from
https://github.com/totalspectrum/spin2cpp/releases. (Make sure you get
version 3.7.0 or later.) Unzip it to a folder on your hard
drive. You'll get two files: fastspin.exe (the program) and
fastspin.md (the documentation). For convenience you may want to add
the folder containing fastspin.exe to your system PATH; or, you can
just copy fastspin.exe into the folder you're working in.

Using Fastspin to Convert to PASM
---------------------------------

This is very easy. For example, to convert a Spin object Fibo.spin
to a PASM object that can run in another COG, called Fibo.cog.spin,
just do:

```
   fastspin -w Fibo.spin
```

in a command line (Windows or Linux or Mac).  This produces a file
Fibo.cog.spin. You now can use "Fibo.cog" in place of "Fibo" and get
the speed benefits.

The only other thing you have to do is add a call to the `__cognew`
method to actually start the object up. (We have to use the special
`__cognew` method because the Spin cognew function only supports
executing bytecode methods.)


### A complete example

Here's Fibo.spin:
```
PUB fibo(n)
  if (n < 2)
    return n
  return fibo(n-1) + fibo(n-2)
```

and here is fibodemo.spin, which uses it:
```
'' fibodemo.spin: demonstrate COGOBJ with a Fibonacci calculator
'' runs both bytecode and PASM versions of the fibo function

CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  ser: "FullDuplexSerial"
  bytecode: "Fibo"
  pasm: "Fibo.cog"
  
PUB hello | e1, e2, i, n, n2
  pasm.__cognew
  ser.start(31, 30, 0, 115200)
  repeat i from 1 to 10
    e1 := CNT
    n := bytecode.fibo(i)
    e1 := CNT - e1
    e2 := CNT
    n2 := pasm.fibo(i)
    e2 := CNT - e2
    ser.str(string("fibo("))
    ser.dec(i)
    ser.str(string(") = "))
    ser.dec(n)
    ser.str(string(" bytecode time "))
    ser.dec(e1)
    ser.str(string(" / pasm time "))
    ser.dec(e2)
    ser.str(string(" cycles", 13, 10))
    if ( n <> n2)
      ser.str(string("  ERROR", 13, 10))
```

Note that you can mix regular Spin objects and COG objects freely, and
in fact can use the original Fibo.spin alongside the converted
Fibo.cog.spin.

The `__cognew` method was automatically added by `fastspin` when it
translated blinker.spin from Spin to PASM. It does all the
housekeeping involved with getting communication going between the
Spin code and the PASM code running on another processor. Similarly
there's a `__cogstop` method which will stop the COG. You can also use
the regular Spin `cogstop` function, but this may leave the code in a
state which makes it hard to start again, so `__cogstop` is better.


To compile and run this on the command line I do:

```
fastspin -w Fibo.spin
openspin fibodemo.spin
propeller-load fibodemo.binary -r -t
```

You can also use Fibo.cog.spin with any Spin IDE or other Spin tools.
It should be legal for all Spin compilers.

The output looks like:
```
fibo(1) = 1 bytecode time 2272 / pasm time 11536 cycles
fibo(2) = 1 bytecode time 6368 / pasm time 11536 cycles
fibo(3) = 2 bytecode time 10464 / pasm time 12640 cycles
fibo(4) = 3 bytecode time 18656 / pasm time 12640 cycles
fibo(5) = 5 bytecode time 30944 / pasm time 13744 cycles
fibo(6) = 8 bytecode time 51424 / pasm time 15952 cycles
fibo(7) = 13 bytecode time 84192 / pasm time 18160 cycles
fibo(8) = 21 bytecode time 137440 / pasm time 22576 cycles
fibo(9) = 34 bytecode time 223456 / pasm time 29200 cycles
fibo(10) = 55 bytecode time 362720 / pasm time 41344 cycles
```

### Performance

Some things to note about the performance:

(1) There's a fixed overhead of nearly 10000 cycles for managing the
inter-COG communication and getting the answer back from the remote COG.
So for small calculations the PASM isn't worth it.

(2) Once we start getting slightly more complicated calculations the PASM
speed quickly becomes apparent. In this case we can see that the PASM
approaches 10x faster than regular Spin bytecode.

Blinking Leds
-------------

Another classic test. Here's a simple pin blinking object. The pin and time
between blinks are given as a parameter:

```
'' blinker.spin
PUB run(pin, pausetime)
  DIRA[pin] := 1
  repeat
    OUTA[pin] ^= 1
    waitcnt(CNT+pausetime)

```
Tweak pin to be something that's actually wired up to an LED,
and pausetime to the number of cycles you want to pause.

To use this in a program do something like:
```
'' blinkdemo.spin
'' blink an LED in another COG
'' COGSPIN version
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  mypin = 15
  mypause = 40_000_000
OBJ
  fds: "FullDuplexSerial"
  blink: "blinker.cog"
  
PUB demo | id
  fds.start(31, 30, 0, 115200)
  id := blink.__cognew
  blink.run(mypin, mypause)
  fds.str(string("blink running in cog "))
  fds.dec(id)
  fds.tx(13)
  fds.tx(10)
  repeat
```

Convert blinker.spin to the PASM blinker.cog.spin as usual...

```
fastspin -w blinker.spin
```

...and then you can compile and run `blinkdemo.spin` in the Propeller
tool of your choice. In my case it's the command line:

```
   fastspin -w blinker.spin
   openspin -q blinkdemo.spin
   propeller-load blinkdemo.binary -r -t
```

Synchronous and Asynchronous Operation
--------------------------------------

If you've been watching carefully you've noticed that the fibo demo
got results back from the PASM COG (i.e. the Spin COG waited for the PASM
COG to finish) but the blink demo did not (the blinking ran alongside
the Spin COG). The first case is "synchronous" operation, and the
second is "asynchronous". You may wonder how fastspin knew to wait in
one case and not in the other. The answer is simple: if a method
returns a value, fastspin adds code to make the Spin bytecode wait for
the PASM's result. If the method never returns a value (and never
assigns to the `result` variable) then there's no wait, and the Spin
bytecode can continue on its way.

You can use asynchronous operation to allow the Spin code to do work
even while a calculation is in progress. The trick is to start the
computation in a method that returns nothing, and then to provide a
"getter" method that actually returns the result. For example:

```
'' multiply all N elements in an array by a number
'' and return the resulting sum of them
VAR
  long answer
  
'' start the array scaling operation
'' we want this to run asynchronously,
'' so do NOT return the result, just store
'' it in the variable "answer"

PUB scaleArray(arrptr, n, scale) | i, t, sum
  sum := 0
  repeat i from 0 to n-1
    t := long[arrptr][i] * scale
    long[arrptr][i] := t
    sum += t
  answer := sum

'' here's the getter
PUB getAnswer
  return answer
```

Now on the Spin COG side we can launch the computation with the
`scaleArray` method, then do some work and come back later to get the
answer with `getAnswer`. If instead of `answer := sum` we had done
`return sum` or `result := sum` then `scaleArray` would have caused
the Spin COG to wait for the result.

A More Real World Example
-------------------------

```
''
'' Trivial Serial port written in Spin
''
'' this is for a very simple serial port
'' (transmit only, and only on the default pin for now)
''
CON
  txpin = 30
  
VAR
  long bitcycles
   
PUB start(baudrate)
  bitcycles := clkfreq / baudrate
  return 1
  
PUB tx(c) | val, nextcnt
  OUTA[txpin] := 1
  DIRA[txpin] := 1

  val := (c | 256) << 1
  nextcnt := CNT
  repeat 10
     waitcnt(nextcnt += bitcycles)
     OUTA[txpin] := val
     val >>= 1

PUB str(s) | c
  REPEAT WHILE ((c := byte[s++]) <> 0)
    tx(c)
```

This simple serial port, in the original Spin, works correctly and is
fine for output at low speeds. But if you convert it to PASM then it
will transmit at 921600 baud, and perhaps higher -- I haven't actually
tried to see how high it will go.

Granted, we already have FullDuplexSerial.spin and a host of other
serial ports. But this TrivialSerial is really easy to understand and
to modify, so if you need to change something to communicate with a
non-standard device, well, this would be easier than trying to tweak
somebody else's PASM code :).

### The Generated Code

The .cog.spin produced by fastspin is somewhat readable, and we can look in
it to see how the compiler does. For example, the code generated for
the `tx` method of the trival serial driver looks like:
```
pasm_Tx
	movs	_Tx_ret, #doreturn
_Tx
	mov	_Tx__mask_0001, imm_1073741824_
	or	OUTA, imm_1073741824_
	or	DIRA, imm_1073741824_
	or	arg1, #256
	shl	arg1, #1
	mov	_Tx_Val, arg1
	mov	_Tx_Nextcnt, CNT
	mov	_Tx__idx__0002, #10
L__0021
	rdlong	Tx_tmp002_, objptr
	add	_Tx_Nextcnt, Tx_tmp002_
	mov	arg1, _Tx_Nextcnt
	waitcnt	arg1, #0
	shr	_Tx_Val, #1 wc
	muxc	OUTA, _Tx__mask_0001
	djnz	_Tx__idx__0002, #L__0021
_Tx_ret
	ret
```
which isn't too bad. We could improve it by keeping the bitcycles counter
in a local variable, which would avoid the read from memory in the inner
loop. Or we could tweak the generated assembly by hand.

### Behind the Scenes

Some other things you will note in the .cog.spin file:

  * The compiler guesses at the size of stack needed for the object
    and adds a constant `__STACK_SIZE` at the top of the file to say how
    big to make it (in longs). This is a conservative guess, and if
    memory is tight you may be able to reduce this size. Conversely, if
    you have recursive functions you may find you need more space and
    can increase it here. If you add a definition for `__STACK_SIZE` to
    your original .spin file, fastspin will notice it and use that value
    instead of the one it calculates. (`__STACK_SIZE` is basically the
    same as the Spin constant `_STACK`, but is used for PASM code instead
    of bytecode.)
  * There's also a define for __MBOX_SIZE, giving the size of the mailbox
    used to communicate with the COG. This one is exact, don't try to
    change it.
  * The first two items in the mailbox are a global lock and a function code.
    The global lock is to make sure only one Spin COG at a time uses the PASM
    COG. This feature hasn't been tested very much yet. The function code
    indicates what function the PASM COG is running. If it is 0 it means the
    PASM COG is idle and is able to accept new commands.

Restrictions
------------

There are a few restrictions on .cog.spin mode:

(1) coginit and cognew cannot be used to start Spin methods in a .cog.spin file. They can start assembly code, if you choose to insert some PASM in your .spin, but it must be PASM that you wrote, not generated automatically by the compiler.

(2) Obviously, your code must fit into the memory of a single COG

(3) Variables in the VAR section will be placed in HUB memory. Local variables in Spin methods will usually be in COG memory, unless the @ operator is applied to them.
