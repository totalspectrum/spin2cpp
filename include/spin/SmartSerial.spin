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
  long cur_bitrate

'' if baudrate of -1 is given, auto baud detection is employed

PUB start(rxpin, txpin, mode, baudrate) | bitperiod, txmode, rxmode, bit_mode
  if baudrate == -1
    bitperiod := autobaud(rxpin)
  else
    bitperiod := (CLKFREQ / baudrate)

  dirl_(txpin)
  dirl_(rxpin)
  cur_bitrate := bitperiod
  bit_mode := 7 + (bitperiod << 16)
  rx_pin := rxpin
  tx_pin := txpin
  txmode := _txmode
  rxmode := _rxmode
  wrpin_(txmode, txpin)
  wxpin_(bit_mode, txpin)
  dirh_(txpin)
  wrpin_(rxmode, rxpin)
  wxpin_(bit_mode, rxpin)
  dirh_(rxpin)

  return 1

PUB getrate
  return cur_bitrate

PRI autobaud(pin) | a, b, c, delay, port, mask
  dirl_(pin)   ' set pin low
  waitx_(1000) ' wait to settle
  if pin => 32
    port := 1
    mask := 1<<(pin-32)
  else
    port := 0
    mask := 1<<pin

  ' code for detecting HI->LO->HI->LO transition
  ' we time the length of the first 1 bit sequence in the character,
  ' then the next 0 bit sequence
  ' we assume one of these is the correct length
  ' this works if the character sent is space ($20), which has 1 bit high
  ' or CR ($0d) which has 1 bit low after the high bit
  '
  asm
    test port, #1 wc	' set C to distinguish INA/OUTA
    test port, #2 wz    ' set Z (match on =)

    setpat mask, #0	' wait for pin lo (start bit)
    waitpat
    setpat mask, mask	' wait for pin hi (first 1)
    waitpat
    getct a
    setpat mask, #0	' wait for pin lo again (following 0)
    waitpat
    getct b
    setpat mask,mask	' wait for pin hi again (end of 0 sequence)
    waitpat
    getct c
  endasm
  delay := b - a	' length of first 1 bit sequence
  c := c - b            ' length of following 0
  if c < delay
    delay := c

  ' now want to wait for idle
  waitx_(16*delay)
  return delay
  
' start with default serial pins and mode
PUB start_default(baudrate)
  return start(63, 62, 0, baudrate)
  
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
#include "spin/std_text_routines.spinh"
