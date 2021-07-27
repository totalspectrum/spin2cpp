''
'' Nucode specific functions
''

'' for now, dummies
pri _txraw(c)
  return 1

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
pri _fltl(pin)
  __bytecode__("FLTL")


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
