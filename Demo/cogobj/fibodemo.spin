#define USE_COG_SER

CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
#ifdef USE_COG_SER
  ser: "SimpleSerial.cog"
#else
  ser: "SimpleSerial"
#endif
  fibo: "Fibo"
  fibo2: "Fibo.cog"
  
PUB hello | elapsed, e2, i, n, n2
  pause(1)
#ifdef USE_COG_SER
  ser.__cognew
#endif
  ser.start(115200)
  ser.str(string("Fibonacci demo...", 13, 10))
  
  fibo2.__cognew
  repeat i from 1 to 16
    ser.str(string("fibo("))
    ser.decuns(i, 2)
    ser.str(string(") = "))
    elapsed := CNT
    n := fibo.fibo(i)
    elapsed := CNT - elapsed

    ser.decuns(n, 4)

    e2 := CNT
    n2 := fibo2.fibo(i)
    e2 := CNT - e2
    
    ser.str(string(" elapsed time "))
    ser.decuns(elapsed, 8)
    ser.str(string(" / "))
    ser.decuns(e2, 8)
    ser.str(string(" cycles", 13, 10))
    if ( n <> n2)
      ser.str(string("  ERROR", 13, 10))
    
PUB pause(n)
  waitcnt(CNT+n*8_000_000)

