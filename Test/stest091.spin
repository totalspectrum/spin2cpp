OBJ
  fds0: "simplepin"       ' offset 0
  fds1: "simplepin"       ' offset 4
  fdsArr[4]: "simplepin"  ' offset 8, 12, 16, 20
  fdsType = "simplepin"   ' abstract type, no space occupied

PUB sendchar1(c)
  fds1.tx(c)
  
PUB sendchar2(c)
  fdsArr[2].tx(c)

PUB sendchar_index(i, c)
  fdsArr[i].tx(c)
  
PUB sendchar_abstract(fds, c)
  fdsType[fds].tx(c)

  

