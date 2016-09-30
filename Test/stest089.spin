
pub main | time
  time := cnt
  repeat 512
    OUTA ^= 1
  time := cnt - time
  DIRA := time
  