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
  ser.start(115200)
  repeat i from 1 to 16
    elapsed := CNT
    n := fibo.calc(i)
    elapsed := CNT - elapsed
    e2 := CNT
    n2 := fibo2.calc(i)
    e2 := CNT - e2
    ser.str(string("fibo("))
    ser.decuns(i, 2)
    ser.str(string(") = "))
    ser.decuns(n, 4)
    ser.str(string(" elapsed time "))
    ser.decuns(elapsed, 8)
    ser.str(string(" / "))
    ser.decuns(e2, 8)
    ser.str(string(" cycles", 13, 10))
    if ( n <> n2)
      ser.str(string("  ERROR", 13, 10))
    
PUB pause(n)
  waitcnt(CNT+n*8_000_000)

