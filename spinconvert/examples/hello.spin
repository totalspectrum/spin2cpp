''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x
  txpin = 30
  rxpin = 31
  baud = 115200
  txmask = (1<<txpin)
  bitcycles = _clkfreq / baud  
   
PUB demo
''   serstr(string("hello, world!", 13, 10))
  repeat
     serchar("H")
     serchar("I")
     serchar("!")
     serchar(13)
     serchar(10)
     
PUB serchar(c) | val, waitcycles
  OUTA |= txmask
  DIRA |= txmask
  val := (c | 256) << 1
  waitcycles := CNT
  repeat 10
     waitcnt(waitcycles += bitcycles)
     if (val & 1)
       OUTA |= txmask
     else
       OUTA &= !txmask
     val >>= 1

PUB serstr(s) | c
  REPEAT WHILE ((c := byte[s++]) <> 0)
    serchar(c)
