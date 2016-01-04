''
'' test of address operators
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  
OBJ
  fds : "FullDuplexSerial.spin"

PUB main | start,elapsed,idx
  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  fds.str(string("Str1 is: "))
  fds.str(@Str1)
  fds.str(string("Str2 is: "))
  fds.str(@Str2)
  fds.str(string("Str3 is: "))
  fds.str(@Str3)
  fds.str(string("indirect with @@:", 13, 10))
  repeat idx from 0 to 2
    fds.str(@@strAddr[idx])
#ifdef TRIPLEAT
  fds.str(string("indirect with @@@:", 13, 10))
  repeat idx from 0 to 2
    fds.str(strAddr2[idx])
#endif
  exit

PUB exit
  fds.txflush
  fds.stop

DAT
  Str1 byte "Hello.", 13, 10, 0
  Str2 byte "This is an example ", 0
  Str3 byte "of strings in a DAT block.", 13, 10, 0

DAT
  StrAddr word @Str1, @Str2, @Str3
#ifdef TRIPLEAT
  StrAddr2 long @@@Str1, @@@Str2, @@@Str3
#endif