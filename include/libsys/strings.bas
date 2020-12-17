' retrieve the leftmost n characters from a string
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

' retrieve the rightmost n characters from a string
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

' retrieve the middle substring starting at i and j characters long
function mid$(x as string, i=1, j=9999999) as string
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

' convert an integer to a single character string
function chr$(x as integer) as string
  dim p as ubyte pointer
  p = new ubyte(2)
  if (p) then
    p(0) = x
    p(1) = 0
  end if
  return p
end function

class __strs_cl
  dim p as ubyte pointer
  dim i as integer
  function pfunc(c as integer) as integer
    if (i < 16) then
      p(i) = c
      i = i+1
      return 1
    else
      return -1
    end if
  end function
end class

' format a floating point number as a string
function str$(x as single) as string
  dim p as ubyte pointer
  dim i as integer
  dim g as __strs_cl pointer
  p = new ubyte(15)
  i = 0
  if p then
    g = __builtin_alloca(8)
    g(0).p = p
    g(0).i = i
    _fmtfloat(@g(0).pfunc, 0, x, ASC("g"))
  end if
  '' FIXME: should we check here that i is sensible?
  return p
end function

'
' convert a number val in base B to a string with n digits
' if n == 0 then we just use enough digits to fit
'
function number$(val as uinteger, n as uinteger, B as uinteger) as string
  dim s as ubyte ptr
  dim d as uinteger
  dim tmp as uinteger
  dim lasttmp as uinteger
  
  if n = 0 then
     ' figure out how many digits we need
     n = 1
     tmp = B
     lasttmp = 1
     ' we have to watch out for overflow in very large
     ' numbers; if tmp wraps around (so tmp < last tmp)
     ' then stop
     while tmp <= val and lasttmp < tmp
       lasttmp = tmp
       tmp = tmp * B
       n = n + 1
     end while
  end if
  ' for 32 bit numbers we'll never need more than 32 digits
  if n > 32 then
    n = 32
  endif
  s = new ubyte(n+1)
  s[n] = 0
  while n > 0
    n -= 1
    d = val mod B
    val = val / B
    if d < 10 then
      d = d + ASC("0")
    else
      d = (d - 10) + ASC("A")
    endif
    s[n] = d
  end while
  return s
end function

function hex$(v as uinteger, n=0 as uinteger)
  return number$(v, n, 16)
end function

function bin$(v as uinteger, n=0 as uinteger)
  return number$(v, n, 2)
end function

function decuns$(v as uinteger, n=0 as uinteger)
  return number$(v, n, 10)
end function

function oct$(v as uinteger, n=0 as uinteger)
  return number$(v, n, 8)
end function

