dim x(15, 127) as integer
type mat as integer(15, 127)

function fetchx(i, j) as integer
  return x(i, j)
end function

function fetchm1(i, j, m as Mat) as integer
  return m(i,j)
end function

function fetchm2(i, j, m as integer(15,127)) as integer
  return m(i,j)
end function

function fetchm3(i, j, m(15) as integer(127)) as integer
  return m(i,j)
end function

function fetchm4(i, j, m(15,127) as integer) as integer
  return m(i,j)
end function
