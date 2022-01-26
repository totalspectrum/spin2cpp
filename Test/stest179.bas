#define ARR_BASE 0
#define SIZE5 5 + ARR_BASE - 1
#define SIZE7 7 + ARR_BASE - 1

option base ARR_BASE

dim shared as ubyte a(SIZE5,SIZE7)
dim b(SIZE5, SIZE7) as ubyte

function get01() as integer
  ' fetch offset 1
  return a(0 + ARR_BASE, 1 + ARR_BASE) + b(0 + ARR_BASE, 1 + ARR_BASE)
end function

function get10() as integer
  ' fetch offset 7
  return a(1 + ARR_BASE, 0 + ARR_BASE) + b(1 + ARR_BASE, 0 + ARR_BASE)
end function

function get23() as integer
  ' fetch offset 2*7 + 3 == 17
  return a(2 + ARR_BASE, 3 + ARR_BASE) + b(2 + ARR_BASE, 3 + ARR_BASE)
end function
