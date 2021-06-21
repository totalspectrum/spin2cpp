'' check for clock frequency setting

CON
#ifndef __P2__
  _clkmode = xtal1 + pll4x
  '_clkmode = xtal1 + pll16x ' should fail
#endif  
  _clkfreq = 20_000_000
  '_clkfreq = 80_000_000 ' should fail

OBJ
#ifdef __P2__
  fds : "spin/SmartSerial"
#else
  fds : "FullDuplexSerial"
#endif

PUB main | start,elapsed
  '' start up the serial port
#ifdef __P2__  
  fds.start(63, 62, 0, 230400)
#else  
  fds.start(31, 30, 0, 115200)
#endif

  start := CNT
  '' transmit (12*10) = 120 characters, or about 1200 bits
  '' at 115200 bps, that should take 0.01 seconds
  '' at 80 MHz that is 800000 cycles
  '' at 20 MHz it is 200000 cycles

  repeat 12
    fds.str(string("ABCDEFGH", 13, 10))
    
  fds.txflush
  elapsed := CNT
  elapsed := elapsed - start

  if elapsed < 800000
     fds.str(string("time is short, as expected", 13, 10))
     fds.txflush
  else
     fds.str(string("elapsed="))
     fds.dec(elapsed)
     fds.str(string(13, 10))
     fds.txflush
  exit

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat
