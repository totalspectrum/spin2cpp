' set contents of a to lesser of b and c
DAT
filler
	byte 0[760]
modus_name
	byte 0

PUB set1(c)
    byte[@modus_name + 20][0] := c

PUB set2(c)
    byte[@modus_name][20] := c


  