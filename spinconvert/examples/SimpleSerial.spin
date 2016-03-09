''
'' serial port definitions
''
{{CON
  txpin = 30
  rxpin = 31
  baud = 115200
  txmask = (1<<txpin)
  bitcycles = _clkfreq / baud
  }}
VAR
  byte txpin
  byte rxpin
  long baud
  long txmask
  long bitcycles
   
''
'' code: largely taken from FullDuplexSerial.spin
''
PUB start(rx_pin, tx_pin, mode, baudrate)
  baud := baudrate
  bitcycles := clkfreq / baudrate
  txpin := tx_pin
  txmask := (1<<txpin)
  rxpin := rx_pin
  
PUB tx(c) | val, waitcycles
  OUTA |= txmask
  DIRA |= txmask
  val := (c | 256) << 1
  waitcycles := CNT
  repeat 10
     waitcnt(waitcycles += bitcycles)
     if (val & 1)
       OUTA |= txmask
     else
       OUTA &= !txmask
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


''
'' could signal back to the host with an exit code
'' for now don't
''
PUB exit(code)
  repeat
