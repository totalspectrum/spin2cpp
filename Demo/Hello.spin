''
'' simple hello world that works on PC or Propeller
''
'' to compile for PC:
''   spin2cpp --cc=gcc -DPC --elf -o Hello HelloWorld.spin
'' to compile for Propeller
''   spin2cpp --asm --binary -Os -o hello.binary HelloWorld.spin
'' to compiler for Prop2:
''   spin2cpp --p2 --asm -o hello.p2asm --code=hub HelloWorld.spin
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  
VAR
  byte txpin
  byte rxpin
  long baud
  long txmask
  long bitcycles
   
#ifdef PC
'' we will be using stdio, so force it to
'' be included
{++
#include <stdio.h>
#include <stdlib.h>
}
#endif

''
'' print out hello, world
''
PUB hello
  start(31, 30, 0, 115200)
  repeat
    str(string("hello, world!", 13, 10))
  
''
'' code: largely taken from FullDuplexSerial.spin
''

PUB start(rx_pin, tx_pin, mode, baudrate)
#ifndef PC
  baud := baudrate
  bitcycles := _clkfreq / baudrate
  txpin := tx_pin
  txmask := (1<<txpin)
  rxpin := rx_pin
#endif
  return 1
  
PUB tx(c) | val, waitcycles, mask
#ifdef PC
  '' just emit direct C code here
  {++
  putchar(c);
  }
#else
  mask := txmask
# ifdef __P2__
  OUTB |= mask
  DIRB |= mask
# else
  OUTA |= mask
  DIRA |= mask
#endif
  val := (c | 256) << 1
  waitcycles := CNT
  repeat 10
     waitcnt(waitcycles += bitcycles)
#ifdef __P2__
     if (val & 1)
       OUTB |= mask
     else
       OUTB &= !mask
     val >>= 1
#else
     if (val & 1)
       OUTA |= mask
     else
       OUTA &= !mask
     val >>= 1
#endif
#endif

PUB str(s) | c
  REPEAT WHILE ((c := byte[s++]) <> 0)
    tx(c)


