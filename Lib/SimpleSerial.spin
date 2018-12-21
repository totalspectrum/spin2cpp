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
  long rxmask
  long bitcycles
   
#ifdef PC
'' we will be using stdio, so force it to
'' be included
{++
#include <stdio.h>
#include <stdlib.h>
}
#else
#ifdef __P2__
#define DIRR DIRB   
#define OUTR OUTB
#define INR  INB
#else
#define DIRR DIRA
#define OUTR OUTA
#define INR  INA
#endif

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
  rxmask := (1<<rxpin)
#endif
  return 1
  
PUB tx(c) | val, waitcycles
#ifdef PC
  '' just emit direct C code here
  {++
  putchar(c);
  }
#else
  OUTR |= txmask
  DIRR |= txmask
  val := (c | 256) << 1
  waitcycles := CNT + bitcycles
  repeat 10
     waitcycles += bitcycles
     waitcnt(waitcycles)
     if (val & 1)
       OUTR |= txmask
     else
       OUTR &= !txmask
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

PUB rx | waitcycles, cycles, mask, val, x
#ifdef PC
  {++
    val = getchar();
  }
#else
  mask := rxmask
  cycles := bitcycles
  DIRR &= !mask  ' set for input
  '' wait for start bit
  repeat
    x := INR
  while ( (x & mask) <> 0 )
  val := $0
  waitcycles := CNT + (cycles >> 1)  '' sync for one half bit
  repeat 8
      val := val >> 1
      waitcnt(waitcycles += cycles)
      x := INR
      if ( (x & mask) <> 0 )
        val |= $80
  '' wait for stop bit?
  '' skip it for now
  '' waitpeq(mask, mask, 0)
#endif
  return val

''
'' could signal back to the host with an exit code
'' for now don't
''
PUB exit(code)
  repeat
