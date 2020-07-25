'' Does not compile
PUB main | tmp1, tmp2
  tmp1 := constant(@something2-@something1)
  outa := tmp1
  tmp2 := @something2 - 2
  outb := tmp2
  
DAT
something1
long 1
something2
long 42
