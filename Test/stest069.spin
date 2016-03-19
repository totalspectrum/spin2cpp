PUB demo

  repeat 7
    PauseABit
  exit

PUB exit
  repeat

PUB PauseABit
  waitcnt(40_000_000 + cnt)
