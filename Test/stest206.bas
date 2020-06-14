function clamp(x)
  if (x > 127) x = 127
  if (x < 0)   x = 0
  return x
end function
