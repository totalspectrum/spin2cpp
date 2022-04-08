/**
 * @brief Interface Driver for SD cards
 * @author Michael Burmeister
 * @date Feburary 1, 2021
 * @version 1.0
 * 
*/

//#define _DEBUG

#include <stdio.h>
#include <propeller.h>
#include "sd_mmc.h"

unsigned long Pins[2];

void SetSD(int drive, int cs, int clk, int mosi, int miso)
{
    Pins[drive] = cs | (clk << 8) | (mosi << 16) | (miso << 24);
}


void EnableSD(int drive)
{
    int cs, clk, mosi, miso;

    _waitms(10);
    cs = Pins[drive] & 0xff;
    clk = (Pins[drive] >> 8) & 0xff;
    mosi = (Pins[drive] >> 16) & 0xff;
    miso = (Pins[drive] >> 24) & 0xff;
    _dirh(cs);
    _pinh(cs);
    _dirh(clk);
    _pinl(clk);
    _dirh(mosi);
    _dirl(miso);
}

void SendSD(int drive, const BYTE* buff, unsigned int bc)
{
    int i = 0;
    const BYTE *b;
    unsigned int x;
    int mosi = (Pins[drive] >> 16) & 0xff;
    int clk = (Pins[drive] >> 8) & 0xff;
    int j = 0;
    int t = bc;

    b = buff;
    __asm const {
            mov   i, t
    loopi   mov   j, #8
            rdbyte x, b
            shl x, #24
    loopj   shl x, #1 wc
            outc mosi
            drvh clk
            nop
            drvl clk
            djnz j, #loopj
            add b, #1
            djnz i, #loopi
      }

}

void ReceiveSD(int drive, BYTE *buff, unsigned int bc)
{
    BYTE *b = buff;
    unsigned int c = bc;
    int mosi = (Pins[drive] >> 16) & 0xff;
    int miso = (Pins[drive] >> 24) & 0xff;
    int clk = (Pins[drive] >> 8) & 0xff;
    int i = 0;
    int j = 0;
    int v = 0;

    __asm const {
           drvh mosi
           mov i, c
    loopi  mov v, #0
           mov j, #8
    loopj  drvh clk
           testp miso wc
           drvl clk
           rcl v, #1
           nop
           nop
           djnz j, #loopj
           wrbyte v, b
           add b, #1
           djnz i, #loopi
    }
}

int GetStatus(int drive)
{
    int i = 0;
    BYTE b = 0;
    
    for (i=0;i<50000;i++)
    {
        ReceiveSD(drive, &b, 1);
        if (b == 0xff)
        	return 1;
    }
	return 0;
}

void ReleaseSD(int drive)
{
    int cs;
    BYTE b;
    
    cs = Pins[drive] & 0xff;
    _pinh(cs);
    ReceiveSD(drive, &b, 1);
}

int SelectSD(int drive)
{
    int cs;
    BYTE b;
    
    cs = Pins[drive] & 0xff;
    _pinl(cs);
    ReceiveSD(drive, &b, 1);
    if (GetStatus(drive))
    	return 1;
    ReleaseSD(drive);
    return 0;
}

int ReceiveBlock(int drive, BYTE *buff, unsigned int bc)
{
    BYTE b[2];
    int i;
   
    for (i=0;i<5000;i++)
    {
        ReceiveSD(drive, b, 1);
        if (*b != 0xff)
        	break;
    }
	if (*b != 0xfe)
		return 0;
	ReceiveSD(drive, buff, bc);
	ReceiveSD(drive, b, 2);    // Ditch CRC
	return 1;
}

int SendBlock(int drive, const BYTE *buff, int token)
{
    BYTE b[2];
    
    if (GetStatus(drive) == 0)
    	return 0;
    
    b[0] = token;
    SendSD(drive, b, 1);
    if (token != 0xfd)
    {
        SendSD(drive, buff, 512);
        ReceiveSD(drive, b, 2);  // CRC value
        ReceiveSD(drive, b, 1);
        if ((*b & 0x1f) != 0x05)
        	return 0;
    }
	return 1;    
}

BYTE SendCommand(int drive, BYTE cmd, unsigned int arg)
{
    BYTE b[6];
    BYTE n;
    
    if (cmd & 0x80)
    {
        cmd = cmd & 0x7f;
        n = SendCommand(drive, CMD55, 0);
        if (n > 1)
        	return n;
    }
    
    if (cmd != CMD12)
    {
        ReleaseSD(drive);
        if (!SelectSD(drive))
        	return 0xff;
    }
    
    b[0] = 0x40 | cmd;
    b[1] = arg >> 24;
    b[2] = arg >> 16;
    b[3] = arg >> 8;
    b[4] = arg;
    n = 0x01;
    if (cmd == CMD0)
    	n = 0x95;
    if (cmd == CMD8)
    	n = 0x87;
    b[5] = n;
    SendSD(drive, b, 6);
    
    if (cmd == CMD12)
    	ReceiveSD(drive, &n, 1);

    for (int i=0;i<10;i++)
    {
        ReceiveSD(drive, &n, 1);
        if ((n & 0x80) == 0)
        	break;
    }
#ifdef _DEBUG
    __builtin_printf("Command: %d (%x) --> %d\n", cmd, arg, n);
#endif
    return n;
}
