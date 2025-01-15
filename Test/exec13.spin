CON
#ifdef __P2__
  _clkfreq = 160_000_000
#else
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
#endif

VAR
  long SqStack[64]	' Stack space for Square cog

DAT
astr byte "test text ", 0, 0, 0, 0

OBJ
#ifdef __P2__
  ser: "spin/SmartSerial"
#else
  ser: "spin/FullDuplexSerial"
#endif
  bf: "bftest01.b"

PUB demo | x,y

#ifdef __P2__
  ser.start(63, 62, 0, 230400)
#else
  ser.start(31, 30, 0, 115200)
#endif

  send := @ser.tx
  send("bf test", 13, 10)
  bf.program()
  send(13)
  x := 2
  cognew(Square(@X), @SqStack)
  repeat 7
    ReportN(x)
    PauseABit

  ' test bytemove
  ReportAstr(@"Original: ", @astr)
  ' delete first character
  bytemove(@astr, @astr+1, 9)
  ReportAstr(@"Deleted: ", @astr)
  ' insert another char
  bytemove(@astr+1, @astr, 9)
  astr[0] := "b"
  ReportAstr(@"Inserted: ", @astr)

  PauseABit
  exit

PUB ReportN(n)
  send("x = ")
  ser.dec(n)
  send(13, 10)

PUB ReportAstr(msg, s)
  ser.str(msg)
  ser.str(s)
  ser.str(string(13, 10))
  
PUB Square(XAddr)
 ' Square the value at XAddr
 repeat
   long[XAddr] *= long[XAddr]
   waitcnt(80_000_000 + cnt)
   
PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  ser.tx($ff)
  ser.tx($00)
  ser.tx($00) '' the exit status
#ifndef __P2__
  ser.txflush
#endif
  repeat

PUB PauseABit
  waitcnt(40_000_000 + cnt)
