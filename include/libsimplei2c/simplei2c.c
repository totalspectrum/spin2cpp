/**
 * @file simplei2c.c
 * Provides simple i2c start, stop, send, and receive functions
 *
 * Copyright (c) 2013, Parallax Inc.
 *
 * Modified for FlexC P1/P2 compatibility by Roy Eltham
 *
 */
#include "simplei2c.h"

static inline void scl_low(i2c *bus)
{
    _pinl(bus->scl_pin);
}

static inline void sda_low(i2c *bus)
{
    _pinl(bus->sda_pin);
}

static inline void scl_high(i2c *bus)
{
    if (bus->drivescl)
    {
        _pinh(bus->scl_pin);
    }
    else
    {
        // set to input, assumes pullup will pull high
        _pinl(bus->scl_pin);
        _dirl(bus->scl_pin);
    }
}

static inline void sda_high(i2c *bus)
{
    _pinh(bus->sda_pin);
}

static inline void all_low(i2c *dev)
{
    scl_low(dev);
    sda_low(dev);
}

static inline void all_high(i2c *dev)
{
    sda_high(dev);
    scl_high(dev);
}

i2c *i2c_open(i2c *bus, int sclPin, int sdaPin, int sclDrive)
{
  bus->scl_pin = sclPin;
  bus->sda_pin = sdaPin;
  bus->drivescl = sclDrive;
  i2c_stop(bus);
  return bus;
}

void i2c_start(i2c *bus)
{
    all_high(bus);
    sda_low(bus);
    _waitus(1);
    scl_low(bus);
}

void i2c_stop(i2c *bus)
{
    all_low(bus);
    scl_high(bus);
    _waitus(20);
    sda_high(bus);
}

int i2c_writeByte(i2c *bus, int byte)
{
    int result;
    int count = 8;

    /* send the byte, high bit first */
    do 
    {
        if (byte & 0x80)
        {
            _dirl(bus->sda_pin);
        }
        else
        {
            _dirh(bus->sda_pin);
        }
        scl_high(bus);
        byte <<= 1;
        scl_low(bus);
    } while (--count > 0);
    
    _dirl(bus->sda_pin);
    
    /* get ack */
    scl_high(bus);
    result = _pinr(bus->sda_pin);
    scl_low(bus);
    return result != 0;
}

int i2c_readByte(i2c *bus, int ackState)
{
    int byte = 0;
    int count = 8;

    _dirl(bus->sda_pin);

    for (count = 8; --count >= 0; )
    {
        byte <<= 1;
        scl_high(bus);
        byte |= _pinr(bus->sda_pin) ? 1 : 0;
        while (!_pinr(bus->scl_pin)); // clock stretching
        scl_low(bus);
    }

    /* acknowledge */
    if (ackState)
    {
        _dirl(bus->sda_pin);
    }
    else
    {
        _dirh(bus->sda_pin);
    }
    scl_high(bus);
    _waitus(1);
    scl_low(bus);

    return byte;
}

int i2c_writeData(i2c *bus, const unsigned char *data, int count)
{
    int n  = 0;
    int rc = 0;
    while (count-- > 0)
    {
        rc |= i2c_writeByte(bus, (int) data[n]);
        if (rc)
        {
            return n;
        }
        n++;
    }
    return n;
}

int i2c_readData(i2c *bus, unsigned char *data, int count)
{
    int n = 0;
    while (--count > 0)
    {
        data[n] = (unsigned char) i2c_readByte(bus, 0);
        n++;
    }
    data[n] = (unsigned char) i2c_readByte(bus, 1);
    return n;
}

int i2c_poll(i2c *bus, int devaddr)
{
    int ack = 0;
    i2c_start(bus);
    ack = i2c_writeByte(bus, devaddr);
    return ack;
}

/*
+--------------------------------------------------------------------
| TERMS OF USE: MIT License
+--------------------------------------------------------------------
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
+--------------------------------------------------------------------
*/
