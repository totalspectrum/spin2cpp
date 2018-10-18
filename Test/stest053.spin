''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x
  txpin = 30
  baud = 115200
  txmask = (1<<txpin)
  bitcycles = _clkfreq / baud  
   
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
