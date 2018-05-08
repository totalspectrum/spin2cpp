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
#ifdef __P2__
  clkset($ff, 80_000_000)
#endif
  start(31, 30, 0, 115200)
  repeat
    str(string("hello, world!", 13, 10))
  
''
'' code: largely taken from FullDuplexSerial.spin
''

#ifdef PC
PUB start(rx_pin, tx_pin, mode, baudrate)
  return 1
  
#else

PUB start(rx_pin, tx_pin, mode, baudrate)
  baud := baudrate
  bitcycles := _clkfreq / baudrate
  txpin := tx_pin
  txmask := (1<<txpin)
  rxpin := rx_pin
  
#ifdef __P2__
PUB tx(c) | waitcycles, mask, bcycles
  c := (c|256) << 1
  mask := txmask
  OUTB |= mask
  DIRB |= mask
  waitcycles := CNT
  bcycles := 80000000 / 115200
  repeat 10
    asm
      shr c, #1 wc
      drvc #62
      addct1 waitcycles, bcycles
      waitct1
    endasm
    
#else

PUB tx(c) | val, waitcycles, mask
  mask := txmask
  OUTA |= mask
  DIRA |= mask
  val := (c | 256) << 1
  waitcycles := CNT
  repeat 10
     waitcnt(waitcycles += bitcycles)
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


