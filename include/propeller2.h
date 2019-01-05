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

#define CNT getcnt()

/**
 * @brief getpin accessor used to read the state of a pin.
 *
 * @details P1 provides pin access via registers only.
 * This inline macro provides access to read a given pin.
 *
 * This macro is a convenience for portability between P1/P2 code.
 *
 * @param pin Pin to read in the range 0:63.
 * @returns State of the requested pin with range 0:1.
 */
static __inline__ int getpin(int pin)
{
    int val = 0;
    __asm {
        testp pin wc
        muxc val, #1
    }
    return val;
}

/**
 * @brief setpin accessor used to write the state of a pin.
 *
 * @details P1 provides pin access via registers only.
 * This inline macro provides access to write the value to a given pin.
 *
 * This macro is a convenience for portability between P1/P2 code.
 *
 * @param pin Pin to read in the range 0:63.
 * @param value The value to set to the pin 0:1
 * @returns Nothing.
 */
static __inline__ void setpin(int pin, int value)
{
    if (value) {
        __asm {
            drvh pin
        }
    } else {
        __asm {
            drvl pin
        }
    }
}

/**
 * @brief togglepin accessor used to toggle the state of a pin.
 *
 * @details P1 provides pin access via registers only.
 * This inline macro provides access to toggle the value of a given pin.
 * Toggle means to set the opposite of the existing state.
 *
 * This macro is a convenience for portability between P1/P2 code.
 *
 * @param pin Pin to read in the range 0:31.
 * @returns Nothing.
 */
static __inline__ void togglepin(int pin)
{
    __asm {
        drvnot pin
    }
}
