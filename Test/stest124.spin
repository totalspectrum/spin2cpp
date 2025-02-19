dat
  long
bas_tx_handles
  long 0[8]
  
pub _call_method(o, f, x=0) | r
  asm
    wrlong objptr, sp
    add    sp, #4
    mov    objptr, o
    mov    arg01, x
    call   f
    sub    sp, #4
    rdlong objptr, sp
    mov    r, result1
  endasm
  return r

pub _basic_print_char(h, c) | saveobj, t, f, o
  t := bas_tx_handles[h]
  if t == 0
    return
  o := long[t]
  f := long[t+4]
  if f == 0
    _mytx(c)
  else
    _call_method(o, f, c)


pub _mytx(c) | i
  repeat i from 0 to 4
    outa := c | i
    