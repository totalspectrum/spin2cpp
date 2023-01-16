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

/**
 * @brief CNT register accessor.
 *
 * @details P1 provides a COG accessible CNT register.
 *
 * This macro is a convenience for portability between P1/P2 code.
 *
 * @returns the global CNT value.
 */
#define getcnt() _CNT

/**
 * @brief getpin accessor used to read the state of a pin.
 *
 * @details P1 provides pin access via registers only.
 * This inline macro provides access to read a given pin.
 *
 * This macro is a convenience for portability between P1/P2 code.
 *
 * @param pin Pin to read in the range 0:31.
 * @returns State of the requested pin with range 0:1.
 */
static __inline__ int getpin(int pin)
{
    uint32_t mask = 1 << pin;
    _DIRA &= ~mask;
    return _INA & mask ? 1 : 0;
}

/**
 * @brief setpin accessor used to write the state of a pin.
 *
 * @details P1 provides pin access via registers only.
 * This inline macro provides access to write the value to a given pin.
 *
 * This macro is a convenience for portability between P1/P2 code.
 *
 * @param pin Pin to read in the range 0:31.
 * @param value The value to set to the pin 0:1
 * @returns Nothing.
 */
static __inline__ void setpin(int pin, int value)
{
    uint32_t mask = 1 << pin;
    if (value)
        _OUTA |= mask;
    else
        _OUTA &= ~mask;
    _DIRA |= mask;
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
    uint32_t mask = 1 << pin;
    _OUTA ^= mask;
    _DIRA |= mask;
}


#define cognew(code, data) _coginit(0x10, code, data)
#define locknew() _locknew()
#define lockclr(x) _lockclr(x)
#define lockset(x) _lockset(x)
#define lockret(x) _lockret(x)
#define cogstop(x) _cogstop(x)
