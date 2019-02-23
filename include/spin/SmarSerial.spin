'
' SmartSerial.spin2
' simple smart pin serial object for P2 eval board
' implements a subset of FullDuplexSerial functionality
'
CON
  _txmode       = %0000_0000_000_0000000000000_01_11110_0 'async tx mode, output enabled for smart output
  _rxmode       = %0000_0000_000_0000000000000_00_11111_0 'async rx mode, input  enabled for smart input

VAR
  long rx_pin, tx_pin

PUB start(rxpin, txpin, mode, baudrate) | bitperiod, txmode, rxmode
  bitperiod := 7 + ((CLKFREQ / baudrate) << 16)
  rx_pin := rxpin
  tx_pin := txpin
  txmode := _txmode
  rxmode := _rxmode
  wrpin_(txmode, txpin)
  wxpin_(bitperiod, txpin)
  dirh_(txpin)
  wrpin_(rxmode, rxpin)
  wxpin_(bitperiod, rxpin)
  dirh_(rxpin)
  return 1
  
PUB tx(val) | txpin
  txpin := tx_pin
  wypin_(val, txpin)
  waitx_(1)
  txflush

PUB txflush | txpin, z
  txpin := tx_pin
  z := 1
  repeat while z <> 0
    asm
      testp txpin wc
   if_c mov z, #0
    endasm

' check if byte received (never waits)
' returns -1 if no byte, otherwise byte

PUB rxcheck : rxbyte | rxpin, z
  rxbyte := -1
  rxpin := rx_pin
  asm
    testp rxpin wc  ' char ready?
    if_c rdpin rxbyte, rxpin
    if_c shr rxbyte, #24
  endasm

' receive a byte (waits until one ready)
PUB rx : v
  repeat
    v := rxcheck
  while v == -1


'' provide the usual str(), dec(), etc. routines
#include "std_text_routines.spinh"
