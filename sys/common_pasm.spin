''
'' common code that applies to both P1 and P2 PASM
''
  
pri longmove(dst, src, count=long) : origdst
  origdst := dst
  if dst < src
    repeat count
      long[dst] := long[src]
      dst += 4
      src += 4
  else
    dst += 4*count
    src += 4*count
    repeat count
      dst -= 4
      src -= 4
      long[dst] := long[src]
      
pri wordmove(dst, src, count=long) : origdst
  origdst := dst
  if dst < src
    repeat count
      word[dst] := word[src]
      dst += 2
      src += 2
  else
    dst += 2*count
    src += 2*count
    repeat count
      dst -= 2
      src -= 2
      word[dst] := word[src]

pri bytemove(dst, src, count=long)
  return __builtin_memmove(dst, src, count)

pri __builtin_memcpy(dst, src, count=long)
  return __builtin_memmove(dst, src, count)

pri __builtin_strlen(str) : r=long
  r := str
  repeat while byte[r] <> 0
    r++
  r -= str

pri __builtin_strcpy(dst, src) : r=@byte | c
  r := dst
  repeat
    c := byte[src++]
    byte[dst++] := c
  until c==0

pri strcomp(s1, s2) | c1, c2
  repeat
    c1 := byte[s1++]
    c2 := byte[s2++]
    if (c1 <> c2)
      return 0
  until (c1 == 0)
  return -1

pri _lookup(x, b, arr, n=long) | i
  i := x - b
  if (i => 0 and i < n)
    return long[arr][i]
  return 0

pri _lookdown(x, b, arr, n=long) | i
  repeat i from 0 to n-1
    if (long[arr] == x)
      return i+b
    arr += 4
  return 0

pri _lfsr_forward(x) | a
  if (x == 0)
    x := 1
  a := $8000000b
  repeat 32
    asm
      test x, a wc
      rcl  x, #1
    endasm
  return x
  
pri _lfsr_backward(x) | a
  if (x == 0)
    x := 1
  a := $17
  repeat 32
    asm
      test x, a wc
      rcr  x, #1
    endasm
  return x
  
pri __getsp | x
  asm
    mov x, sp
  endasm
  return x

pri __topofstack(ptr)
  return @ptr

pri __get_heap_base : r
  asm
    mov r, __heap_ptr
  endasm

pri _lockmem(addr) | r, mask
  mask := _cogid() + $100
  repeat
    asm_const
         rdlong r, addr wz
  if_z   wrlong mask, addr
  if_z   rdlong r, addr
  if_z   rdlong r, addr
    endasm
  until r == mask

pri _unlockmem(addr) | oldlock
  long[addr] := 0
 
