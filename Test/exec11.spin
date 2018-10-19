'' check for clock frequency setting

CON
  _clkmode = xtal1 + pll4x
  _clkfreq = 20_000_000
''  _clkmode = xtal1 + pll16x
''  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

PUB main | start,elapsed
  '' start up the serial port
  fds.start(31, 30, 0, 115200)

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

  if elapsed < 300000
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


  
