#ifndef __P2__
#error This driver is for P2 only
#endif

'
' SmartSerial.spin2
' simple smart pin serial object for P2 eval board
' implements a subset of FullDuplexSerial functionality
' This version uses some fastspin features, so it will not
' work on PNut.
'
' Written by Eric R. Smith
' Copyright 2020 Total Spectrum Software Inc.
' Distributed under the MIT License (See LICENSE.md)
'
' methods:
' start(baud):
'   start serial on pins 63 and 62 at given baud rate
' startx(rxpin, txpin, flags, baud):
'   start on pins rxpin,txpin and given baud rate; `flags` is ignored
'   and is provided for backwards compatibility. if rxpin or txpin is -1,
'   that pin is ignored
' tx(c)
'   send character `c` (must call start or startx first)
' rxcheck()
'   see if a character is available; returns -1 if no character, otherwise
'   returns the character
' rx()
'   waits for a character to become available, then returns it
'
con
  _txmode       = %0000_0000_000_0000000000000_01_11110_0 'async tx mode, output enabled for smart output
  _rxmode       = %0000_0000_000_0000000000000_00_11111_0 'async rx mode, input  enabled for smart input

var
  long rx_pin, tx_pin

'' There are two ways we might call start():
'' start(rxpin, txpin, mode, baudrate) - FullDuplexSerial way
'' start(baud)                         - PST way
''
'' fastspin can handle this by giving default parameter values;
'' if a "txpin" of -99 is detected, then we assume the second way
'' was called
''
'' otherwise a negative value for a pin means no receive (or no transmit)
''
'' so to just set up transmit on pin 6 at 115_200 baud do
''    start(6, -1, 0, 115_200)
''
pub start(rxpin, txpin=-99, mode=0, baudrate=230400)
  if txpin == -99
     baudrate := rxpin
     rxpin := 63
     txpin := 62
     
  startx(rxpin, txpin, mode, baudrate)

pub startx(rxpin, txpin, mode, baudrate) | bitperiod, bit_mode
  bitperiod := (clkfreq / baudrate)

  ' save parameters in the object
  rx_pin := rxpin
  tx_pin := txpin

  ' calculate smartpin mode for 8 bits per character
  bit_mode := 7 + (bitperiod << 16)

  ' set up the transmit pin
  if txpin => 0
    pinf(txpin)
    wrpin(txpin, _txmode)
    wxpin(txpin, bit_mode)
    pinl(txpin)	' turn smartpin on by making the pin an output

  ' set up the receive pin
  if rxpin => 0
    pinf(rxpin)
    wrpin(rxpin, _rxmode)
    wxpin(rxpin, bit_mode)
    pinl(rxpin)  ' turn smartpin on

' transmit the 8 bit value "val"
pub tx(val)
  if tx_pin => 0
    wypin(tx_pin, val)
    txflush()

pub txflush() | z
  repeat
    z := pinr(tx_pin)
  while z == 0
  
' check if byte received (never waits)
' returns -1 if no byte, otherwise byte

pub rxcheck() : rxbyte | rxpin, z
  rxbyte := -1
  rxpin := rx_pin
  if rxpin => 0
    z := pinr(rxpin)
    if z
      rxbyte := rdpin(rxpin)>>24
       

' receive a byte (waits until one ready)
pub rx() : v
  repeat
    v := rxcheck()
  while v == -1


'' provide the usual str(), dec(), etc. routines
#include "spin/std_text_routines.spinh"
