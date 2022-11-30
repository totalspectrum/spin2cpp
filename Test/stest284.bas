dim x(1 to 16, 1 to 128) as integer
type mat as integer(1 to 16, 1 to 128)

function fetchx(i, j) as integer
  return x(i, j)
end function

function fetchm1(i, j, m as Mat) as integer
  return m(i,j)
end function

function fetchm2(i, j, m as integer(1 to 16, 1 to 128)) as integer
  return m(i,j)
end function

function fetchm3(i, j, m(1 to 16) as integer(1 to 128)) as integer
  return m(i,j)
end function

function fetchm4(i, j, m(1 to 16, 1 to 128) as integer) as integer
  return m(i,j)
end function
