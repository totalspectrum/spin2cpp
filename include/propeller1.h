#pragma once

/// Parameter register is used for sharing HUB RAM address info with the COG.
#define PAR     _PAR
/// The system clock count
#define CNT     _CNT
/// Use to read the pins when corresponding DIRA bits are 0.
#define INA     _INA
/// Unused in P8X32A
#define INB     _INB
/// Use to set output pin states when corresponding DIRA bits are 1.
#define OUTA    _OUTA
/// Unused in P8X32A
#define OUTB    _OUTB
/// Use to set pins to input (0) or output (1).
#define DIRA    _DIRA
/// Unused in P8X32A
#define DIRB    _DIRB
/// Counter A control register.
#define CTRA    _CTRA
/// Counter B control register.
#define CTRB    _CTRB
/// Counter A frequency register.
#define FRQA    _FRQA
/// Counter B frequency register.
#define FRQB    _FRQB
/// Counter A phase accumulation register.
#define PHSA    _PHSA
/// Counter B phase accumulation register.
#define PHSB    _PHSB
/// Video Configuration register can be used for other special output.
#define VCFG    _VCFG
/// Video Scale register for setting pixel and frame clocks.
#define VSCL    _VSCL

#define getcnt() _CNT
