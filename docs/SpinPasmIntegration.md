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

This is very easy. Just download fastspin.zip from
https://github.com/totalspectrum/spin2cpp/releases. (Make sure you get
version 3.7.0 or later.) Unzip it to a folder on your hard
drive. You'll get two files: fastspin.exe (the program) and
fastspin.md (the documentation). For convenience you may want to add
the folder containing fastspin.exe to your system PATH; or, you can
just copy fastspin.exe into the folder you're working in.

Blinking Leds
-------------

For our first program, use your favorite text editor (notepad++, vim,
and emacs are all good ones, but any text editor or IDE will do) and
create a new file called blinker.spin. It'll contain the classic LED
blinking code, as follows:

```
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  pin = 15
  pausetime = _clkfreq/2

PUB run
  DIRA[pin] := 1
  repeat
    OUTA[pin] ^= 1
    waitcnt(CNT+pausetime)

```
Tweak the _clkmode and _clkfreq if necessary for your system; also change pin to a pin that's actually wired up to an LED!

This should run as is. Now, suppose we want to run this task as PASM
code in another cog (this is overkill, obviously, but it's just an
example!). To convert this module into a PASM object, open a Windows
command shell and cd to the directory that holds blinker.spin. Make
sure you have fastspin.exe in that directory too (or on your system
PATH), Now enter

   fastspin -w blinker.spin

This will create a new file blinker.cog.spin that contains the
translated PASM code, plus some Spin wrappers.

To use this in a project, create blink0.spin which contains:
```
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  b: "blinker.cog"

PUB demo
  b.__cognew
  b.run
  repeat
```

The `b.__cognew` line actually launches the object's code in a new
COG. In a bigger program we would check the return value (which is the
COG's id, or -1 if it failed).  `b.run` starts the `run` method
on the new COG, which blinks the LED.

The `__cognew` method was automatically added by `fastspin` when it
translated blinker.spin from Spin to PASM. It does all the
housekeeping involved with getting communication going between the
Spin code and the PASM code running on another processor. There's also
a `__cogstop` method which will stop the COG. You can also use the
regular Spin `cogstop` function, but this may leave the code in a
state which makes it hard to start again, so `__cogstop` is better.


## Passing parameters and getting results

Let's change `blinker.spin` so that it accepts the pin number and
pause time between blinks as parameters, and also passes back a count
of the number of blinks that have happened. We'll leave the `run`
method alone (for backwards compatibility) but add a new `blink`
method with more flexibility. Here's the new blinker.spin:
```
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  default_pin = 15
  default_pausetime = _clkfreq/2

PUB run | x
  blink(default_pin, default_pausetime, @x)
  
PUB blink(pin, pausetime, countptr)
  DIRA[pin] := 1
  repeat
    OUTA[pin] ^= 1
    long[countptr]++
    if (pausetime)
      waitcnt(CNT+pausetime)
```

And here's blink1.spin, which runs this and uses FullDuplexSerial to report
back to us:
```
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  mypin = 15
  
OBJ
  b: "blinker.cog"
  ser: "FullDuplexSerial"
  
PUB demo | id, count
  ser.start(31, 30, 0, 115200)
  id := b.__cognew
  ser.str(string("running on cog "))
  ser.dec(id)
  ser.str(string(13, 10))
  count := 0
  b.blink(mypin, _clkfreq, @count)
  waitcnt(CNT+_clkreq)
  ser.dec(count)
  ser.str(string("blinks in the first second", 13, 10))
  ser.str(string(" press a key to see the count of blinks", 13, 10))
  repeat
    ser.rx
    ser.dec(count)
    ser.str(string(13, 10))
    ser.blink
```

We can see in this program that the main Spin code can keep running while
the converted Spin code in the other COG does the blinking.

Remember to re-convert blinker.spin to PASM:

   fastspin -w blinker.spin

And then compile and run blink1.spin with your favorite Spin IDE. For
me, it's the command line:

   fastspin -w blinker.spin
   openspin blink1.spin
   propeller-load blink1.binary -r -t

## Performance

So far we haven't done anything that couldn't be done just as easily
as in plain Spin. But the performance is very different. Change
blink1.spin so that it passes a 0 for the pausetime argument of
`b.blink`, instead of `_clkfreq`. This will show how many blinks per
second the PASM version can print. An entirely Spin version of the
code would look like:

```
```

Try running this, and compare the performance numbers. For me, I get:


Compute Engine
--------------

Another place PASM is great at is speeding up calculations.
