''
'' serial port definitions
''
'' this is for a very simple serial port
'' also note: if PC is defined then we
'' substitute some C code instead
''
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
'' code: largely taken from FullDuplexSerial.spin
''

PUB start(rx_pin, tx_pin, mode, baudrate)
#ifndef PC
  baud := baudrate
  bitcycles := clkfreq / baudrate
  txpin := tx_pin
  txmask := (1<<txpin)
  rxpin := rx_pin
#endif
  return 1
  
PUB tx(c) | val, waitcycles
#ifdef PC
  '' just emit direct C code here
  {++
  putchar(c);
  }
#else
#ifdef __P2__
  OUTB |= txmask
  DIRB |= txmask
#else
  OUTA |= txmask
  DIRA |= txmask
#endif
  val := (c | 256) << 1
  waitcycles := CNT
  repeat 10
     waitcnt(waitcycles += bitcycles)
#ifdef __P2__
     if (val & 1)
       OUTB |= txmask
     else
       OUTB &= !txmask
#else
     if (val & 1)
       OUTA |= txmask
     else
       OUTA &= !txmask
#endif
     val >>= 1
#endif

PUB str(s) | c
  REPEAT WHILE ((c := byte[s++]) <> 0)
    tx(c)

PUB dec(value) | i, x

'' Print a decimal number
  result := 0
  x := value == NEGX                                                            'Check for max negative
  if value < 0
    value := ||(value+x)                                                        'If negative, make positive; adjust for max negative
    tx("-")                                                                     'and output sign

  i := 1_000_000_000                                                            'Initialize divisor

  repeat 10                                                                     'Loop for 10 digits
    if value => i                                                               
      tx(value / i + "0" + x*(i == 1))                                          'If non-zero digit, output digit; adjust for max negative
      value //= i                                                               'and digit from value
      result~~                                                                  'flag non-zero found
    elseif result or i == 1
      tx("0")                                                                   'If zero digit (or only digit) output it
    i /= 10                                                                     'Update divisor

PUB hex(val, digits) | shft, x
  shft := (digits - 1) << 2
  repeat digits
    x := (val >> shft) & $F
    shft -= 4
    if (x => 10)
      x := (x - 10) + "A"
    else
      x := x + "0"
    tx(x)


''
'' could signal back to the host with an exit code
'' for now don't
''
PUB exit(code)
  repeat
