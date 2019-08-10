function left$(x as string, n as integer) as string
  dim p as ubyte pointer
  dim i, m as integer

  if (n <= 0) return ""
  m = __builtin_strlen(x)
  if (m <= n) return x
  p = new ubyte(n+1)
  if (p) then
    bytemove(p, x, n)
    p(n) = 0
  end if
  return p
end function

function right$(x as string, n as integer) as string
  dim p as ubyte pointer
  dim i, m as integer

  if (n <= 0) return ""
  m = __builtin_strlen(x)
  if (m <= n) return x
  p = new ubyte(n+1)
  if (p) then
    i = m - n
    bytemove(p, @x(i), n+1)
  end if
  return p
end function

function mid$(x as string, i=0, j=9999999) as string
  dim p as ubyte pointer
  dim m, n
  if (j <= 0) return ""
  i = i-1 ' convert from 1 based to 0 based
  m = __builtin_strlen(x)
  if (m < i) return ""

  ' calculate number of chars we will copy
  n = (m-i)
  if (n > j) then
    n = j
  endif
  p = new ubyte(n+1)
  if p then
    bytemove(p, @x(i), n)
    p(n) = 0
  end if
  return p
end function

function chr$(x as integer) as string
  dim p as ubyte pointer
  p = new ubyte(2)
  if (p) then
    p(0) = x
    p(1) = 0
  end if
  return p
end function

function str$(x as single) as string
  dim p as ubyte pointer
  dim i as integer
  var fn = function(c as integer) as integer
    if (i < 31) then
      p(i) = c
      i = i+1
      return 1
    else
      return -1
    end if
  end function
  p = new ubyte(32)
  i = 0
  if p then
    _fmtfloat(fn, 0, x, ASC("#"))
  end if
  return p
end function
