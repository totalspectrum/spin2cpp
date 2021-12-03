pub test1up() | i
  repeat i from 1 to 8
    outa ^= i

pub test1dn() | i
  repeat i from 8 to 1
    outa ^= i

pub test2up() | i
  repeat i from 1 to 8
    outa ^= 1

pub test2dn() | i
  repeat i from 8 to 1
    outa ^= 1

