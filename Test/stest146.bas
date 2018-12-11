option base 2

dim shared as integer A(0 to 2)  ' starts at 0
dim shared as integer B(1 to 2)  ' starts at 12
dim shared as integer C(2)       ' starts at 20

function getA() as integer
  return A(2)  ' offset
end function

function getB() as integer
  return B(2)
end function

function getC() as integer
  return C(2)
end function

