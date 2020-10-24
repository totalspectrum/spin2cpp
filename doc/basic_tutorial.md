# FlexBASIC tutorial

This is a simple tutorial showing some of the ways to blink an LED in
FlexBASIC

## do loop

Here's the most straightforward way to blink a pin in BASIC, using
the good old `do` loop:
```
' doloop.bas
' declare which pin the LED is connected to
const LED=16
' set it as an output
direction(LED) = output

' loop forever
do
  output(LED) = 1 ' set the pin high
  pausems(1000)    ' wait 1 second
  output(LED) = 0 ' set the pin low
  pausems(1000)    ' wait another second
loop
```

`const LED=16` is a line declaring that `LED` is a constant with the value `16`. Thus later when we write `output(LED)` it's the same as writing `output(16)`.

We could have writtne `var LED=16`. This would make `LED` a variable, which could change. However, if we never plan to change the value of `LED` it's better to declare it a constant, because then the can produce better code.

The line `direction(LED) = output` declares that the pin connected to `LED` should be an output. Since we want to drive the pin, it obviously should be an output. If it were connected to a switch or something we wanted to read, we would want to set the direction to `input` instead.

Everything between `do` and `loop` is repeated forever.

`output(LED)` is an expression which describes the current output state of the pin `LED`. If we set it to `1` the pin is driven high. If we set it to `0` the pin is driven low.

`pausems(1000)` is a command to pause for 1000 milliseconds, which is one second. To blink the LED faster we would make this pause smaller, e.g. to blink twice as quickly replace both of the `pausems(1000)` with `pausems(500)`.

## Subroutine version

The program might be easier to understand if we split out the task of toggling the pin into its own set of code, called a "subroutine".
```
' subroutine.bas
' declare which pin we will use
const LED=16
' set it as an output
direction(LED) = output

' declare a subroutine to toggle a pin
sub toggle(pin)
  if output(pin) = 0 then
    output(pin) = 1
  else
    output(pin) = 0
  end if
end sub

' loop forever
do
  toggle(LED)
  pausems(1000)
loop
```

## Subroutine with xor

The `xor` operator does an "exclusive or"; `1 xor 1` is `0`, whereas `0 xor 1` is `1`; this `A xor 1` is the inverse of `A`.

This is exactly what we need to toggle the pin, so we can simplify the `toggle` subroutine as follows:
```
' xor.bas
' declare which pin we will use
const LED=16
' set it as an output
direction(LED) = output

' declare a subroutine to toggle a pin
sub toggle(pin)
  output(pin) = output(pin) xor 1
end sub

' loop forever
do
  toggle(LED)
  pausems(1000)
loop
```

## Old school BASIC

You may have programmed BASIC long ago, when everything had line numbers and programmers used `goto`. This is still supported, although it's no longer recommended:
```
05 rem oldschool.bas
10 let LED=16
20 direction(LED) = output
30 output(LED) = output(LED) xor 1
40 pausems 500
50 goto 30
```

## Functional program

At the other extreme from the old school, here's a recursive and functional definition for the pin toggling program:

```
' functional.bas
const LED = 16

sub forever( f as sub() )
  f()
  forever(f)
end sub

sub toggle()
   output(LED) = output(LED) xor 1
   pausems 500
end sub

direction(LED) = output

forever( toggle )
```

The function `forever` repeats the subroutine `f` forever. It's definition seems a bit brain-twisty at first, but it actually makes a lot of sense. Namely, to repeat `f` forever, first do `f`, then repeat `f` forever. This is a very powerful concept called "recursion".

If you're familiar with other programming languages and how subroutines push their values on the stack, you might wonder how this definition would work -- wouldn't it eventually run out of memory? The answer is that the BASIC compiler is clever enough to notice that the call to `forever` is the last thing in `forever`, and it gets turned into a simple jump instead of a real subroutine call. This is called "tail recursion optimization", and is quite useful.

## Lambda function

You'll note in the example above that the `toggle` subroutine only exists to be passed to another subroutine (`forever`). We can actually just write it inline using special syntax ("lambda notation"):
```
' lambda.bas
const LED = 16

sub forever( f as sub() )
  f()
  forever(f)
end sub

direction(LED) = output

forever( [: output(LED) = output(LED) xor 1 : pausems 100 ] )
```

The general form of a lambda is:
```
[ a, b, c : stuff :=> r ]
```
where:

`a`, `b`, `c` are the inputs to a function (in our original toggle example there were no inputs, so there was nothing before the first `:`);

`stuff` is a sequence of BASIC commands, separated by `:`;

and at the end is the value to return, prefixed by `=>` (in our original example we had a subroutine, which has no return value, so this was left out).
