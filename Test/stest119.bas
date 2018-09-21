function zeroit(a as ubyte ptr, n as uinteger)
  dim i as integer
  for i = 1 to n
    a(i) = 0
  next
end function
