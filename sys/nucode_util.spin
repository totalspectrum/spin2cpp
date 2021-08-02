''
'' Nucode specific functions
''

'' FIXME: these are dummys
pri _rxraw(timeout = 0)
  if timeout
    timeout *= __clkfreq_var >> 10
  return -1

pri _cogchk(id)
  return -1
  
''
'' divide (n, nlo) by d, producing qlo and rlo (used in FRAC operation)
''
pri _div64(n, nlo, dlo) : qlo, rlo | q, r, d
  return 0,0

pri _waitx(tim)
  __bytecode__("WAITX")

pri _pinr(pin) : val
  __bytecode__("PINR")
  
pri _getcnt : r = +long
  __bytecode__("GETCT")

pri _cogid : r = long
  __bytecode__("COGID")

pri _cogstop
  __bytecode__("COGSTOP")

pri _drvl(pin)
  __bytecode__("DRVL")
pri _drvh(pin)
  __bytecode__("DRVH")
pri _drvnot(pin)
  __bytecode__("DRVNOT")
pri _drvrnd(pin)
  __bytecode__("DRVRND")

pri _dirl(pin)
  __bytecode__("DIRL")
pri _dirh(pin)
  __bytecode__("DIRH")

pri _fltl(pin)
  __bytecode__("FLTL")

pri _wrpin(pin, val)
  __bytecode__("WRPIN")
pri _wxpin(pin, val)
  __bytecode__("WXPIN")
pri _wypin(pin, val)
  __bytecode__("WYPIN")
  
pri _waitcnt(x)
  __bytecode__("WAITCNT")
  
dat
    orgh
_rx_temp   long 0

con
 _rxpin = 63
 _txpin = 62

  _txmode       = %0000_0000_000_0000000000000_01_11110_0 'async tx mode, output enabled for smart output
  _rxmode       = %0000_0000_000_0000000000000_00_11111_0 'async rx mode, input  enabled for smart input

pri _setbaud(baudrate) | bitperiod, bit_mode
  bitperiod := (__clkfreq_var / baudrate)
  _dirl(_txpin)
  _dirl(_rxpin)
  long[$1c] := baudrate
  bit_mode := 7 + (bitperiod << 16)
  _wrpin(_txpin, _txmode)
  _wxpin(_txpin, bit_mode)
  _wrpin(_rxpin, _rxmode)
  _wxpin(_rxpin, bit_mode + 20)  ' async using 28 bits instead of 8
  _dirh(_txpin)
  _dirh(_rxpin)
  
pri _txraw(c) | z
  if long[$1c] == 0
    _setbaud(__default_baud__)  ' set up in common.c
  _wypin(_txpin, c)
  _waitx(1)
  repeat
    z := _pinr(_txpin)
  while z == 0
  return 1

'
' memset(): we may want to optimize this to use longfill in special cases?
'
pri __builtin_memset(ptr, val, count) : r
  r := ptr
  bytefill(ptr, val, count)

pri _lockmem(addr) | oldlock, oldmem, lockreg
  lockreg := __getlockreg
  repeat
    repeat
      oldlock := _lockset(lockreg)
    while oldlock
    oldmem := byte[addr]
    if oldmem == 0
      long[addr] := 1
    _lockclr(lockreg)
  while oldmem <> 0

pri _unlockmem(addr) | oldlock
  long[addr] := 0

pri __topofstack(ptr)
  return @ptr
