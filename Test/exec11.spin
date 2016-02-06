'' check for clock frequency setting

CON
  _clkmode = xtal1 + pll2x
  _clkfreq = 10_000_000
''  _clkmode = xtal1 + pll16x
''  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

PUB main | start,elapsed
  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  start := CNT
  '' transmit 1152 characters, or about 72 bits
  '' at 115200 bps, that should take 0.1 seconds
  '' at 80 MHz that is 8000000 cycles
  '' at 10 MHz it is 1000000 cycles

  repeat 16
    fds.str(string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGH", 13, 10))
    
  fds.txflush
  elapsed := CNT
  elapsed := elapsed - start

  if elapsed < 9000000
     fds.str(string("time is short, as expected", 13, 10))
     fds.txflush
  else
     fds.str(string("elapsed="))
     fds.dec(elapsed)
     fds.str(string(13, 10))
     fds.txflush
  exit

PUB exit
  fds.txflush
  fds.stop

  
