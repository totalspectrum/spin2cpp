''
'' test of address operators
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  
OBJ
  fds : "spin/FullDuplexSerial"

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
  fds.str(string("indirect with @@@:", 13, 10))
  repeat idx from 0 to 2
    fds.str(strAddr2[idx])
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

DAT
  Str1 byte "Hello.", 13, 10, 0
  Str2 byte "This is an example ", 0
  Str3 byte "of strings in a DAT block.", 13, 10, 0

DAT
  StrAddr word @Str1, @Str2, @Str3
  StrAddr2 long @@@Str1, @@@Str2, @@@Str3
