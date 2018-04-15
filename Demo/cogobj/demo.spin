CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  ''ser: "debug.cog"
  ser: "SimpleSerial.cog"
  fibo: "Fibo"
  fibo2: "Fibo.cog"
  
PUB hello | elapsed, e2, i, n, n2
  ser.__cognew
  fibo2.__cognew
  ser.start(31, 30, 0, 115200)
  ser.str(string("hello, world!", 13, 10))
  repeat i from 1 to 12
    elapsed := CNT
    n := fibo.calc(i)
    elapsed := CNT - elapsed
    e2 := CNT
    n2 := fibo2.calc(i)
    e2 := CNT - e2
    ser.str(string("fibo("))
    ser.dec(i)
    ser.str(string(") = "))
    ser.dec(n)
    ser.str(string(" elapsed time "))
    ser.dec(elapsed)
    ser.str(string(" / "))
    ser.dec(e2)
    ser.str(string(" cycles", 13, 10))
    if ( n <> n2)
      ser.str(string("  ERROR", 13, 10))
    
PUB pause(n)
  waitcnt(CNT+n*8_000_000)

