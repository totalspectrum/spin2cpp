/*
 * SimpleIDE compatible header file for FlexC
 */

#ifndef SIMPLETOOLS_H_
#define SIMPLETOOLS_H_

#ifdef __propeller2__
#include <propeller2.h>
#define cogstart _cogstart
#else
#include <propeller.h>
#endif
#include <compiler.h>

#include "libsimpletext/simpletext.h"
#include "simplei2c.h"

#define print __builtin_printf
#define printi __builtin_printf
#define putChar(x) __builtin_printf("%c", x)
#define putStr(x) __builtin_printf("%s", x)
#define putDec(x) __builtin_printf("%d", x)
#define putHex(x) __builtin_printf("%x", x)

#define pause(ms) _waitms(ms)

#define high(x) _pinh(x)
#define low(x)  _pinl(x)
int input(int pin) _IMPL("libsimpletools/input.c");
#define toggle(x) _pinnot(x)
#define get_state(x) _pinr(x)
#define set_output(p,v) _pinw((p), (v))
#define set_direction(p,v) _dirw((p), (v))

#define st_msTicks 		(_clockfreq() / 1000)
#define st_usTicks 		(_clockfreq() / 1000000)
#define st_iodt 		st_usTicks
#define st_timeout		(_clockfreq() / 4);
#define st_pauseTicks	st_msTicks;

//void set_io_timeout(long clockTicks) { st_timeout = clockTicks; }
//void set_io_dt(long clockticks) { st_iodt = clockTicks; }

unsigned get_direction(int pin) _IMPL("libsimpletools/getDirection.c");
unsigned get_directions(int startPin, int endPin) _IMPL("libsimpletools/getDirections.c");
unsigned get_output(int pin) _IMPL("libsimpletools/getOutput.c");
unsigned get_outputs(int startPin, int endPin) _IMPL("libsimpletools/getOutputs.c");
unsigned get_states(int endPin, int startPin) _IMPL("libsimpletools/getStates.c");
void set_directions(int endPin, int startPin, unsigned int pattern) _IMPL("libsimpletools/setDirections.c");
void set_outputs(int endPin, int startPin, unsigned int pattern) _IMPL("libsimpletools/setOutputs.c");

long count(int pin, long duration, int pinToCount = -1) _IMPL("libsimpletools/count.c");

void dac_ctr(int pin, int channel, int dacVal) _IMPL("libsimpletools/dac.c");
void dac_ctr_res(int bits) _IMPL("libsimpletools/dac.c");
void dac_ctr_stop(void) _IMPL("libsimpletools/dac.c");

void freqout(int pin, int msTime, int frequency) _IMPL("libsimpletools/freqout.c");

int pwm_start(unsigned int cycleMicroseconds) _IMPL("libsimpletools/pwm.c");
void pwm_set(int pin, int channel, int tHigh) _IMPL("libsimpletools/pwm.c");
void pwm_stop(void) _IMPL("libsimpletools/pwm.c");

long pulse_in(int pin, int state) _IMPL("libsimpletools/pulseIn.c");
void pulse_out(int pin, int time) _IMPL("libsimpletools/pulseOut.c");

long rc_time(int pin, int state) _IMPL("libsimpletools/rcTime.c");

void square_wave(int pin, int channel, int freq) _IMPL("libsimpletools/squareWave.c");
void square_wave_stop(void) _IMPL("libsimpletools/squareWave.c");
#ifdef __propeller2__
// since P2 doesn't use a cog like on P1, this variant is needed to stop specific pins, since square_wave_stop() will only stop the last pin started.
void square_wave_stop_pin(int pin) _IMPL("libsimpletools/squareWave.c");
#else
// this one is only used/needed for P1
void square_wave_setup(int pin, int freq, int* _ctr, int* _frq) _IMPL("libsimpletools/squareWave.c");
#endif
int int_fraction(int a, int b, int shift) _IMPL("libsimpletools/squareWave.c");

// Constants for shift_in & shift_out
#define   MSBPRE     0
#define   LSBPRE     1
#define   MSBPOST    2
#define   LSBPOST    3
#define   LSBFIRST   0
#define   MSBFIRST   1

int shift_in(int pinDat, int pinClk, int mode, int bits) _IMPL("libsimpletools/shiftIn.c");
void shift_out(int pinDat, int pinClk, int mode, int bits, int value) _IMPL("libsimpletools/shiftOut.c");

// Variable set by i2c_newbus.
extern unsigned int st_buscnt;

i2c *i2c_newbus(int sclPin, int sdaPin, int sclDrive) _IMPL("libsimpletools/i2c_init.c");
int i2c_out(i2c *busID, int i2cAddr, int memAddr, int memAddrCount, const unsigned char *data, int dataCount) _IMPL("libsimpletools/i2c_out.c");
int i2c_in(i2c *busID, int i2cAddr, int memAddr, int memAddrCount, unsigned char *data, int dataCount) _IMPL("libsimpletools/i2c_in.c");
int i2c_busy(i2c *busID, int i2cAddr) _IMPL("libsimpletools/i2c_busy.c");

// Variables set/used by ee_config
extern i2c *st_eeprom;
extern int st_eeInitFlag;

#define EEPROM_ADDR	 0x50

#define ee_put_byte ee_putByte
#define ee_get_byte ee_getByte
#define ee_put_int ee_putInt
#define ee_get_int ee_getInt
#define ee_put_str ee_putStr
#define ee_get_str ee_getStr
#define ee_put_float32 ee_putFloat32

void ee_config(int sclPin, int sdaPin, int sclDrive) _IMPL("libsimpletools/eeprom_initSclDrive.c");
void ee_init() _IMPL("libsimpletools/eeprom_init.c");
void ee_putByte(unsigned char value, int addr) _IMPL("libsimpletools/eeprom_putByte.c");
char ee_getByte(int addr) _IMPL("libsimpletools/eeprom_getByte.c");
void ee_putInt(int value, int addr) _IMPL("libsimpletools/eeprom_putInt.c");
int ee_getInt(int addr) _IMPL("libsimpletools/eeprom_getInt.c");
void ee_putStr(unsigned char *s, int n, int addr) _IMPL("libsimpletools/eeprom_putStr.c");
unsigned char* ee_getStr(unsigned char* s, int n, int addr) _IMPL("libsimpletools/eeprom_getStr.c");
void ee_putFloat32(float value, int addr) _IMPL("libsimpletools/eeprom_putFloat.c");
float ee_getFloat32(int addr) _IMPL("libsimpletools/eeprom_getFloat.c");

float constrainFloat(float value, float min, float max) _IMPL("libsimpletools/constrainFloat.c"); 
int constrainInt(int value, int min, int max) _IMPL("libsimpletools/constrainInt.c");

void endianSwap(void *resultAddr, void *varAddr, int byteCount) _IMPL("libsimpletools/endianSwap.c");

float mapFloat(float value, float fromMin, float fromMax, float toMin, float toMax) _IMPL("libsimpletools/mapFloat.c");
int mapInt(int value, int fromMin, int fromMax, int toMin, int toMax) _IMPL("libsimpletools/mapInt.c");

int random(int limitLow, int limitHigh) _IMPL("libsimpletools/random.c");

// cog functions
void cog_end(int *coginfo) _IMPL("libsimpletools/cogend.c");
int  cog_num(int *coginfo) _IMPL("libsimpletools/cognum.c");
int* cog_run(void (*function)(void *par), int stacksize) _IMPL("libsimpletools/cogrun.c");

#endif
