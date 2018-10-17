dim val as integer

function getval()
  return val
end function

function setval(n)
  if n == 0 then
    throw "cannot set value to 0"
  endif
  val = n
  return val
end function

function incval(n)
  return setval(val + n)
end function
