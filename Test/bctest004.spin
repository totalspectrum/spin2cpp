VAR
  long arr[123]


PUB direct(i)
outa := arr[0]
outa := arr[1]
outa := arr[i]

PUB override(i)
outa := arr.byte[0]
outa := arr.byte[1]
outa := arr.byte[i]

PUB addrof1(i)

outa := byte[@arr[i]][1]
outa := byte[@arr[i]][0] ' <- bugs used to be here

pub addrof2(i)

outa := byte[@arr[0]][i]
outa := byte[@arr[1]][i] ' <- currently not optimal

pub addroff_const
outa := byte[@arr[0]][0]
outa := byte[@arr[0]][1]
outa := byte[@arr[1]][0] ' <- currently not optimal
outa := byte[@arr[1]][1] ' <- currently not optimal
