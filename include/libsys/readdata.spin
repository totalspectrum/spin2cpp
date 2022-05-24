'' read a line of data from handle h
pri _basic_read_line(h=0) | c, ptr, s, n, maxn, sawcr
  n := 0
  maxn := 24
  sawcr := 0
  ptr := _gc_alloc_managed(maxn)
  if ptr == 0
    return ptr
  repeat
    c := _basic_get_char(h)
    if c =< 0
      quit
    if (c == 10)
      if sawcr and n > 0
        --n
      quit
    if (c == 13)
      sawcr := 1
    else
      sawcr := 0
    if (c == 8) or (c == 127) ' backspace
      if n > 0
        --n
      next
    byte[ptr+n] := c
    n++
    if n == maxn
      s := _gc_alloc_managed(maxn + 32)
      if (s == 0)
        return s
      bytemove(s, ptr, maxn)
      maxn += 32
      _gc_free(ptr)
      ptr := s
  byte[ptr+n] := 0
  return ptr

'' find the end of a string, or the next comma
pri _basic_find_terminator(ptr) | c, inquote
  if ptr == 0
    return ptr
  inquote := 0
  repeat
    c := byte[ptr]
    if c == "," and inquote == 0
      return ptr
    if c == 0 or c == "," or c == 10
      return ptr
    if byte[ptr] == $22 ' (quote)
      inquote := 1 - inquote
      if inquote == 0
        return ptr
    ptr++
    
'' read a string from another string
'' returns two values: the new string, and a pointer into
'' the original string right after where we stopped reading

pri _basic_get_string(src) | n, ptr, endptr
  repeat while byte[src] == " "
    src++
  endptr := _basic_find_terminator(src)
  if endptr == 0
    return (endptr, src)
  n := endptr - src
  ptr := _gc_alloc_managed(n+1)
  if ptr == 0
    return (ptr, src)
  if byte[src] == $22 ' quoted string
    ' skip the leading quote
    bytemove(ptr, src+1, n-1)
    src += n
    ' if there is a quote at the end, overwrite it with trailing 0
    if n > 0
      --n
  else
    bytemove(ptr, src, n)
    src += n
  byte[ptr+n] := 0
  if byte[src]
    src++
  return (ptr, src)
  
'' read an integer from a string
'' returns (value,  new_string_pointer)

pri _basic_get_integer(src = "") : r=long, ptr
  r := __builtin_atoi(src)
  ptr := _basic_find_terminator(src)
  if byte[ptr]
    ptr++

'' read a float from a string
pri _basic_get_float(src = "") : r=float, ptr
  r := __builtin_atof(src)
  ptr := _basic_find_terminator(src)
  if byte[ptr]
    ptr++
