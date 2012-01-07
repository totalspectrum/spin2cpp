VAR
  long flag, cols
  long screenptr

PUB test(c)
  case flag
    $00: 
        case c
           $09: repeat
                  outa := cols++
                while cols & 7
           $0D: outa := c
           other: outa := flag
    $0A: cols := c // cols
  flag := 0
