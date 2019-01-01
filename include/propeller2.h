#pragma once

/// Use to set pins 0-31 to input (0) or output (1).
#define DIRA    _DIRA
/// Use to set pins 32-63 to input (0) or output (1).
#define DIRB    _DIRB

/// Use to read the pins when corresponding DIRA bits are 0.
#define INA     _INA
#define INB     _INB
/// Use to set output pin states when corresponding DIRA bits are 1.
#define OUTA    _OUTA
#define OUTB    _OUTB
