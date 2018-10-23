const PI_2 = 1.570796327
const PI_SCALE = (65536.0 / PI_2)

'
' calculate sin(x) where x is an angle in
' 16.16 fixed point, where a whole circle (2pi) is
' 4.0 (so pi/2 == 1.0)
'
' the ROM lookup table covers the range
' in the range 0 to 2048 (=0x800) (= pi/2)
'
' returns a value from 0 to 0xffff (1.0)
'
function _isin(x as integer) as integer
  dim sinptr as ushort pointer
  dim a as any
  dim xfrac as uinteger
  dim sval, nextsval as uinteger
  dim q as uinteger

  a = 0xe000 ' pointer to sin table in ROM
  sinptr = a

  ' first get x into the correct quadrant
  q = (x >> 16) and 3 ' quadrant 0, 1, 2, 3
  if (q and 1) then
    x = -x
  endif
  x = x and 0xffff ' remove extra circles
  xfrac = x and (not 0xffe0)
  x = x >> 5
  sval = sinptr(x+1)
  ' linear interpolation
  nextsval = sinptr(x+2)
  nextsval = ((nextsval - sval) * xfrac) >> 5
  sval = sval + nextsval

  if (q and 2) then
    sval = -sval
  endif
  return sval
end function


function sin(x as single) as single
  dim s as single
  x = x * PI_SCALE
  s = _isin(x)
  return s / 0xffff
end function
  
function cos(x as single) as single
  return sin(x + PI_2)
end function
