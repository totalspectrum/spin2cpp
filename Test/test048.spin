PUB setcolors(colorptr) | i, fore, back
  repeat i from 0 to 7
    fore := byte[colorptr][i << 1] << 2
    outa[7..0] := fore

PUB stop
  outa[7..0] := 0
