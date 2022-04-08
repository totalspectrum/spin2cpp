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

unsigned int Pins;

void SetSD(int cs, int clk, int mosi, int miso)
{
    Pins = cs | (clk << 8) | (mosi << 16) | (miso << 24);
}


void EnableSD(void)
{
    int cs, clk, mosi, miso;

    cs = Pins & 0xff;
    clk = (Pins >> 8) & 0xff;
    mosi = (Pins >> 16) & 0xff;
    miso = (Pins >> 24) & 0xff;

    _waitms(10);
    _dirh(cs);
    _pinh(cs);
    _dirh(clk);
    _pinl(clk);
    _dirh(mosi);
    _dirl(miso);
}

void SendSD(const char* buff, unsigned int bc)
{
    int i;
    const char *b;
    char x;
    int mosi = (Pins >> 16) & 0xff;
    int clk = (Pins >> 8) & 0xff;
    int j;
    int t = bc;

    b = buff;
    __asm const {
            mov   i, t
    loopi   mov   j, #8
            rdbyte x, b
            shl x, #24
    loopj   shl x, #1  wc
            outc mosi
            drvh clk
            nop
            drvl clk
            djnz j, #loopj
            add b, #1
            djnz i, #loopi
      }
}

void ReceiveSD(char *buff, unsigned int bc)
{
    char *b = buff;
    unsigned int c = bc;
    int mosi = (Pins >> 16) & 0xff;
    int miso = (Pins >> 24) & 0xff;
    int clk = (Pins >> 8) & 0xff;
    int i;
    int j;
    int v;

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

void SendSDB(const char* buff, unsigned int bc)
{
    int i;
    const char *b;
    unsigned int x;
    int mosi = (Pins >> 16) & 0xff;
    int clk = (Pins >> 8) & 0xff;
    int j;
    int t = bc;

    b = buff;
    __asm volatile {
            rdfast #0, b
            mov   i, t
            shr   i, #2
    loopi   mov   j, #32
            rflong x
            movbyts x, #27
    loopj   shl x, #1  wc
            outc mosi
            drvh clk
            nop
            drvl clk
            nop
            djnz j, #loopj
            djnz i, #loopi
      }
}

void ReceiveSDB(char *buff, unsigned int bc)
{
    char *b = buff;
    unsigned int c = bc;
    int mosi = (Pins >> 16) & 0xff;
    int miso = (Pins >> 24) & 0xff;
    int clk = (Pins >> 8) & 0xff;
    int i;
    int j;
    unsigned int v;

    __asm volatile {
           wrfast #0, b
           drvh mosi
           mov i, c
           shr i, #2
    loopi  mov v, #0
           mov j, #32
    loopj  drvh clk
           testp miso wc
           drvl clk
           rcl v, #1
           waitx #4
           djnz j, #loopj
           movbyts v, #27
           wflong v
           djnz i, #loopi
    }
}

int GetStatus(void)
{
    int i;
    char b;
    
    for (i=0;i<50000;i++)
    {
        ReceiveSD(&b, 1);
        if (b == 0xff)
        	return 1;
    }
#ifdef _DEBUG
    __builtin_printf("Drive Not Read\n");
#endif
	return 0;
}

void ReleaseSD(void)
{
    char b;
    
    _pinh(Pins & 0xff);
    ReceiveSD(&b, 1);
}

int SelectSD(void)
{
    char b;
    
    _pinl(Pins & 0xff);
    ReceiveSD(&b, 1);
    if (GetStatus())
    	return 1;
    ReleaseSD();
    return 0;
}

int ReceiveBlock(char *buff, unsigned int bc)
{
    char b[2];
    int i;
   
    for (i=0;i<5000;i++)
    {
        ReceiveSD(b, 1);
        if (*b != 0xff)
        	break;
    }
	if (*b != 0xfe)
		return 0;
	ReceiveSDB(buff, bc);
	ReceiveSD(b, 2);    // Ditch CRC
	return 1;
}

int SendBlock(const char *buff, char token)
{
    char b[2];
    
    if (GetStatus() == 0)
    	return 0;
    
    b[0] = token;
    SendSD(b, 1);
    if (token != 0xfd)
    {
        SendSDB(buff, 512);
        ReceiveSD(b, 2);  // CRC value
        ReceiveSD(b, 1);
        if ((*b & 0x1f) != 0x05)
        	return 0;
    }
	return 1;    
}

char SendCommand(char cmd, unsigned int arg)
{
    char b[6];
    char n;
    
    if (cmd & 0x80)
    {
        cmd = cmd & 0x7f;
        n = SendCommand(CMD55, 0);
        if (n > 1)
        	return n;
    }
    
    if (cmd != CMD12)
    {
        ReleaseSD();
        if (!SelectSD())
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
    SendSD(b, 6);
    
    if (cmd == CMD12)
    	ReceiveSD(&n, 1);

    for (int i=0;i<10;i++)
    {
        ReceiveSD(&n, 1);
        if ((n & 0x80) == 0)
        	break;
    }
#ifdef _DEBUG
    __builtin_printf("Command: %d (%x) --> %d\n", cmd, arg, n);
#endif
    return n;
}
