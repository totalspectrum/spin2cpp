dim shared aa(4) = { 2, 4, -3, -4 }

function total() as integer
  var sum = 0
  for i = 1 to 4
    sum = sum + aa(i)
  next i
  return sum
end function
