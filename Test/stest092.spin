PUB rx(mask, cycles) | x
  DIRA &= !mask
  repeat
    x := INA
  while ((x & mask) <> 0)
  return INA & mask

