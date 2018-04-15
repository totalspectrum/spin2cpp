''
'' serial port definitions
''
'' this is for a very simple serial port
'' (transmit only)
''
VAR
  byte txpin
  long bitcycles
   

''
'' the dec and hex code are largely taken from FullDuplexSerial.spin
''

PUB start(rx_pin, tx_pin, mode, baudrate)
  bitcycles := clkfreq / baudrate
  txpin := tx_pin
  return 1
  
PUB tx(c) | val, nextcnt
  OUTA[txpin] := 1
  DIRA[txpin] := 1

  val := (c | 256) << 1
  nextcnt := CNT
  repeat 10
     waitcnt(nextcnt += bitcycles)
     OUTA[txpin] := val
     val >>= 1

PUB str(s) | c
  REPEAT WHILE ((c := byte[s++]) <> 0)
    tx(c)

PUB dec(value) | i, x

'' Print a decimal number
  result := 0
  x := value == NEGX                                                            'Check for max negative
  if value < 0
    value := ||(value+x)                                                        'If negative, make positive; adjust for max negative
    tx("-")                                                                     'and output sign

  i := 1_000_000_000                                                            'Initialize divisor

  repeat 10                                                                     'Loop for 10 digits
    if value => i                                                               
      tx(value / i + "0" + x*(i == 1))                                          'If non-zero digit, output digit; adjust for max negative
      value //= i                                                               'and digit from value
      result~~                                                                  'flag non-zero found
    elseif result or i == 1
      tx("0")                                                                   'If zero digit (or only digit) output it
    i /= 10                                                                     'Update divisor

PUB hex(val, digits) | shft, x
  shft := (digits - 1) << 2
  repeat digits
    x := (val >> shft) & $F
    shft -= 4
    if (x => 10)
      x := (x - 10) + "A"
    else
      x := x + "0"
    tx(x)

