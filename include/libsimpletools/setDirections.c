/*
 * @file count.c
 *
 * @author Andy Lindsay
 *
 * @version 0.85
 *
 * @copyright Copyright (C) Parallax, Inc. 2012.  See end of file for
 * terms of use (MIT License).
 *
 * @brief count function source, see simpletools.h for documentation.
 *
 * @detail Please submit bug reports, suggestions, and improvements to
 * this code to editor@parallax.com.
 */

#include "simpletools.h"                         // Include simpletools function defs

void set_directions(int endPin, int startPin, unsigned int pattern)
{
  if(endPin < startPin)                       // Handle reverse pin size
  {
    int temp = startPin;
    startPin = endPin;
    endPin = temp;
  }
  int useDirB = false;
  if (startPin > 31)
  {
    startPin -= 32;
    endPin -= 32;
    useDirB = true;
  }  
  unsigned int andMask = -1;
  andMask <<= (31 - (endPin-startPin));
  andMask >>= (31 - endPin);
  andMask = ~andMask;
  unsigned int orMask = pattern << (startPin);
  if (useDirB == true)
  {
    DIRB = (DIRB & andMask) | orMask;
  }
  else
  {
    DIRA = (DIRA & andMask) | orMask;
  }
}

/**
 * TERMS OF USE: MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
