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
   
PUB serchar(c) | v, waitcycles
  OUTA |= txmask
  DIRA |= txmask
  v := (c | 256) << 1
  waitcycles := CNT
  repeat 10
     waitcnt(waitcycles += bitcycles)
     if (v & 1)
       OUTA |= txmask
     else
       OUTA &= !txmask
     v >>= 1
