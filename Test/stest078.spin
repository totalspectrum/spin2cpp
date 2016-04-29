VAR
  long mask

PUB setout
  DIRA |= mask
  OUTA |= mask

