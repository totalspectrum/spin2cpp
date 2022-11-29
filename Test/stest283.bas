dim x(3, 5) as integer
dim y(3) as integer(5)
dim z as integer(3, 5)

function fetchx(i, j) as integer
  return x(i, j)
end function

function fetchy(i, j) as integer
  return y(i, j)
end function

function fetchz(i, j) as integer
  return z(i, j)
end function
