dim val as integer

function getval()
  return val
end function

function setval(n as integer)
  if n == 0 then
    throw "cannot set value to 0"
  endif
  val = n
  return val
end function

function incval(n as integer)
  return setval(val + n)
end function
