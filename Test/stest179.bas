#define BASE 0
#define SIZE5 5 + BASE - 1
#define SIZE7 7 + BASE - 1

option base BASE

dim shared as ubyte a(SIZE5,SIZE7)
dim b(SIZE5, SIZE7) as ubyte

function get01() as integer
  ' fetch offset 1
  return a(0 + BASE, 1 + BASE) + b(0 + BASE, 1 + BASE)
end function

function get10() as integer
  ' fetch offset 7
  return a(1 + BASE, 0 + BASE) + b(1 + BASE, 0 + BASE)
end function

function get23() as integer
  ' fetch offset 2*7 + 3 == 17
  return a(2 + BASE, 3 + BASE) + b(2 + BASE, 3 + BASE)
end function
