const PI = 3.14159265
const PI_2 = 1.570796327
const FIXPT_ONE = 1073741824.0
const PI_SCALE = (FIXPT_ONE / (2.0*PI))

'
' calculate sin(x) where x is an angle in
' 2.30 fixed point, where a whole circle (2pi) is
' 1.0
'
' the ROM lookup table covers the range
' in the range 0 to 2048 (=0x800) (= pi/2)
'
' returns a value from -1.0 to +1.0, again in 2.30 fixed point
'
#ifdef __P2__
function _isin(x as integer) as integer
  dim cx as integer
  dim rx, ry as integer
  x = x<<2 ' convert to 0.32 fixed point
  cx = (1<<30)
  asm
    qrotate cx, x
    getqy ry
  end asm
  return ry
end function
#else
function _isin(x as integer) as integer
  dim sinptr as ushort pointer
  dim a as any
  dim xfrac as uinteger
  dim sval, nextsval as uinteger
  dim q as uinteger

  a = 0xe000 ' pointer to sin table in ROM
  sinptr = a

  ' first get x into the correct quadrant
  q = (x >> 28) and 3 ' quadrant 0, 1, 2, 3
  if (q and 1) then
    x = -x
  endif
  x = (x>>12) and 0x1ffff ' reduce to 0 - 0xffff
  xfrac = x and (not 0x1ffe0)
  x = x >> 5
  sval = sinptr(x+1)
  ' linear interpolation
  nextsval = sinptr(x+2)
  nextsval = ((nextsval - sval) * xfrac) >> 5
  sval = sval + nextsval

  if (q and 2) then
    sval = -sval
  endif
  ' here sval is -0xffff to +0xffff
  ' need to convert
  sval = (sval << 14) / 0xffff
  return sval << 16
end function
#endif

function sin(x as single) as single
  dim s as single
  x = x * PI_SCALE
  s = _isin(x)
  return s / FIXPT_ONE
end function
  
function cos(x as single) as single
  return sin(x + PI_2)
end function

#ifdef TEST
dim sa, ca as any
dim sval, cval as single

pausems 100
print "trig test:"
for x# = 0.0 to PI step 0.1
  sval = sin(x#)
  cval = cos(x#)
'  sa = sval
'  ca = cval
'  print x#, sval, sa, cval, ca
  print x#, sval, cval
next x#
#endif
