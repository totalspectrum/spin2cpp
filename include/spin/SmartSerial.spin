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

'' if baudrate of -1 is given, auto baud detection is employed

PUB start(rxpin, txpin, mode, baudrate) | bitperiod, bit_mode
  if baudrate == -1
    bitperiod := autobaud(rxpin)
  else
    bitperiod := (CLKFREQ / baudrate)

  ' save parameters in the object
  rx_pin := rxpin
  tx_pin := txpin

  ' calculate smartpin mode for 8 bits per character
  bit_mode := 7 + (bitperiod << 16)

  ' set up the transmit pin
  pinf(txpin)
  wrpin(txpin, _txmode)
  wxpin(txpin, bit_mode)
  pinl(txpin)	' turn smartpin on by making the pin an output

  ' set up the receive pin
  pinf(rxpin)
  wrpin(rxpin, _rxmode)
  wxpin(rxpin, bit_mode)
  pinl(rxpin)  ' turn smartpin on


PRI autobaud(pin) | a, b, c, delay, port, mask
  pinf(pin)   ' set pin as input
  waitx(1000) ' wait to settle
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
  org
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
  end
  delay := b - a	' length of first 1 bit sequence
  c := c - b            ' length of following 0
  if c < delay
    delay := c

  ' now want to wait for idle
  waitx(16*delay)
  return delay
  
' start with default serial pins and mode
PUB start_default(baudrate)
  return start(63, 62, 0, baudrate)
  
PUB tx(val)
  wypin(tx_pin, val)
  txflush

PUB txflush() | z
  repeat
    z := pinr(tx_pin)
  while z == 0
  
' check if byte received (never waits)
' returns -1 if no byte, otherwise byte

PUB rxcheck() : rxbyte | rxpin, z
  rxbyte := -1
  rxpin := rx_pin
  z := pinr(rxpin)
  if z
    rxbyte := rdpin(rxpin)>>24
    

' receive a byte (waits until one ready)
PUB rx() : v
  repeat
    v := rxcheck
  while v == -1


'' provide the usual str(), dec(), etc. routines
#include "spin/std_text_routines.spinh"
