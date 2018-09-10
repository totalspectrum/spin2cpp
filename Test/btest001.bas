' this is a comment
'
rem and this is a comment too

function sum%(re, y%)
  rem variable re should not trigger a comment
  rem and neither should remainder
  let remainder = y%
  return re+y%
end function
