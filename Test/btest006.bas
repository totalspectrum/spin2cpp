function testfloat(x as single) as single
  if x > 1 then
    return x
  end if
  return 1
end function

testfloat(2)
