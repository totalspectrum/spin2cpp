/*------------------------------------------------------------------------/
/  Foolproof MMCv3/SDv1/SDv2 (in SPI mode) control module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2019, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------/
  Features and Limitations:

  * Easy to Port Bit-banging SPI
    It uses only four GPIO pins. No complex peripheral needs to be used.

  * Platform Independent
    You need to modify only a few macros to control the GPIO port.

  * Low Speed
    The data transfer rate will be several times slower than hardware SPI.

  * No Media Change Detection
    Application program needs to perform a f_mount() after media change.

/-------------------------------------------------------------------------*/

// Modified for Prop2 by evanh

#include "ff.h"		/* Obtains integer types for FatFs */
#include "diskio.h"	/* Common include file for FatFs and disk I/O layer */

/*-------------------------------------------------------------------------*/
/* Platform dependent macros and functions needed to be modified           */
/*-------------------------------------------------------------------------*/

#include <propeller.h>			/* Include device specific declareation file here */

#ifdef PIN_CLK
#error PIN_CLK definition no longer supported, use _vfs_open_sdcardx instead
#endif
#ifdef PIN_SS
#error PIN_SS definition no longer supported, use _vfs_open_sdcardx instead
#endif

int _pin_clk;
int _pin_ss;
int _pin_di;
int _pin_do;

#ifdef __propeller2__
#define _smartpins_mode_eh /* enable Evanh's fast smartpin code */
#endif

/*
#define PIN_CLK  _pin_clk
#define PIN_SS   _pin_ss
#define PIN_DI   _pin_di
#define PIN_DO  _pin_do
*/

// 
// The values here were tested empirically to work at 300 MHz with some cards
// but it would be nice to have some "reason" for them :)
//
#define PAUSE()      (_waitx(16))
#define SHORTPAUSE() (_waitx(8))

#define DO_INIT()	_fltl(PIN_DO)				/* Initialize port for MMC DO as input */
#define DO		(SHORTPAUSE(), (_pinr(PIN_DO) & 1))	/* Test for MMC DO ('H':true, 'L':false) */

#define DI_INIT()	_dirh(PIN_DI)	/* Initialize port for MMC DI as output */
#define DI_H()		(_pinh(PIN_DI))	/* Set MMC DI "high" */
#define DI_L()		(_pinl(PIN_DI))	/* Set MMC DI "low" */

#define CK_INIT()	_dirh(PIN_CLK)	/* Initialize port for MMC SCLK as output */
#define CK_H()		(_pinh(PIN_CLK), PAUSE())	/* Set MMC SCLK "high" */
#define	CK_L()		(_pinl(PIN_CLK), PAUSE())	/* Set MMC SCLK "low" */

#define CS_INIT()	_dirh(PIN_SS)	/* Initialize port for MMC CS as output */
#define	CS_H()		(_pinh(PIN_SS), PAUSE())	/* Set MMC CS "high" */
#define CS_L()		(_pinl(PIN_SS), PAUSE())	/* Set MMC CS "low" */


static
void dly_us (UINT n)	/* Delay n microseconds (avr-gcc -Os) */
{
    _waitus( n );
}



/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

/* MMC/SD command (SPI mode) */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define CMD13	(13)		/* SEND_STATUS */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

DSTATUS Stat /*= STA_NOINIT*/;	/* Disk status */

BYTE CardType;			/* b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing */



/*-----------------------------------------------------------------------*/
/* Transmit bytes to the card (bitbanging)                               */
/*-----------------------------------------------------------------------*/

static
void xmit_mmc (
	const BYTE* buff,	/* Data to be sent */
	UINT bc				/* Number of bytes to send */
)
{
	int  bc2, d;  // DON'T TOUCH!  Variables are placed in order
	int PIN_CLK = _pin_clk;
	int PIN_DI = _pin_di;
	int PIN_SS = _pin_ss;
	int PIN_DO = _pin_do;

#ifdef _smartpins_mode_eh

// Smartpin SPI transmitter using "continuous" mode and 32-bit word size
//   NOTE: data out always has at least a 4 sysclock lag
	__asm const {		// "const" prevents use of FCACHE
		dirl	PIN_DI		// reset tx smartpin, clears excess data
		setq	#1
		rdlong	bc2, buff	// fetch first data
		rev	bc2
		movbyts	bc2, #0x1b	// endian swap
		wypin	bc2, PIN_DI	// first data to tx shifter

		mov	bc2, bc
		shr	bc, #2  wz	// longword count (rounded down)
		shl	bc2, #3		// bit count (exact)
		wypin	bc2, PIN_CLK	// begin SPI clocks

		dirh	PIN_DI		// liven tx buffer, continuous mode
		add	buff, #8
		rev	d
		movbyts	d, #0x1b	// endian swap
tx_loop
	if_nz	wypin	d, PIN_DI	// data to tx buffer
	if_nz	rdlong	d, buff		// fetch next data
	if_nz	add	buff, #4
	if_nz	rev	d
	if_nz	movbyts	d, #0x1b	// endian swap
tx_wait
	if_nz	testp	PIN_DI  wc	// wait for tx buffer empty
if_nc_and_nz	jmp	#tx_wait
	if_nz	djnz	bc, #tx_loop

// Wait for completion
tx_wait2
		testp	PIN_CLK  wc
	if_nc	jmp	#tx_wait2

		dirl	PIN_DI		// reset tx smartpin to clear excess data
		wypin	##-1, PIN_DI	// TX 0xFF, continuous mode
		dirh	PIN_DI
	}
#else        
	do {
		d = *buff++;	/* Get a byte to be sent */
		if (d & 0x80) DI_H(); else DI_L();	/* bit7 */
		CK_H(); CK_L();
		if (d & 0x40) DI_H(); else DI_L();	/* bit6 */
		CK_H(); CK_L();
		if (d & 0x20) DI_H(); else DI_L();	/* bit5 */
		CK_H(); CK_L();
		if (d & 0x10) DI_H(); else DI_L();	/* bit4 */
		CK_H(); CK_L();
		if (d & 0x08) DI_H(); else DI_L();	/* bit3 */
		CK_H(); CK_L();
		if (d & 0x04) DI_H(); else DI_L();	/* bit2 */
		CK_H(); CK_L();
		if (d & 0x02) DI_H(); else DI_L();	/* bit1 */
		CK_H(); CK_L();
		if (d & 0x01) DI_H(); else DI_L();	/* bit0 */
		CK_H(); CK_L();
	} while (--bc);
#endif        
}



/*-----------------------------------------------------------------------*/
/* Receive bytes from the card (bitbanging)                              */
/*-----------------------------------------------------------------------*/

static
void rcvr_mmc (
	BYTE *buff,	/* Pointer to read buffer */
	UINT bc		/* Number of bytes to receive */
)
{
	int  r, bc2;
	int PIN_CLK = _pin_clk;
	int PIN_DO = _pin_do;

	int PIN_SS = _pin_ss;
	int PIN_DI = _pin_di;

#ifdef _smartpins_mode_eh

// Smartpin SPI byte receiver,
//  SPI clock mode is selected in disk_initialize()
//  NOTE: Has hard coded mode select for post-clock "late" sampling
//
	__asm const {		// "const" prevents use of FCACHE
//	__asm volatile {	// "volatile" prevents automated optimising
//		wrfast	#0, buff
		akpin	PIN_DO			// clear rx buffer
		mov	bc2, bc  wz

// 32-bit rx (WFLONG), about 10% faster than 8-bit code
		shr	bc2, #2  wz
	if_z	jmp	#rx_bytes

		mov	r, bc2
		shl	r, #5			// bit count
		wypin	r, PIN_CLK		// begin SPI clocks
		wxpin	#31 | 32, PIN_DO	// 32 bits, sample after rising clock
rx_loop2
rx_wait2
		testp	PIN_DO  wc		// wait for received byte
	if_nc	jmp	#rx_wait2

		rdpin	r, PIN_DO		// longword from rx buffer
		rev	r
		movbyts	r, #$1b			// endian swap
//		wflong	r
		wrlong	r, buff			// store longword
		add	buff, #4
		djnz	bc2, #rx_loop2

// 8-bit rx (WFBYTE)
rx_bytes
		and	bc, #3  wz		// up to 3 bytes
	if_z	ret

		wxpin	#7 | 32, PIN_DO		// 8 bits, sample after rising clock
rx_loop1
		wypin	#8, PIN_CLK		// begin SPI clocks
rx_wait1
		testp	PIN_DO  wc		// wait for received byte
	if_nc	jmp	#rx_wait1

		rdpin	r, PIN_DO		// byte from rx buffer
		rev	r
//		wfbyte	r			// store byte
		wrbyte	r, buff			// store byte
		add	buff, #1
		djnz	bc, #rx_loop1
	}
#else
	DI_H();	/* Send 0xFF */

	do {
		r = 0;	 if (DO) r++;	/* bit7 */
		CK_H(); CK_L();
		r <<= 1; if (DO) r++;	/* bit6 */
		CK_H(); CK_L();
		r <<= 1; if (DO) r++;	/* bit5 */
		CK_H(); CK_L();
		r <<= 1; if (DO) r++;	/* bit4 */
		CK_H(); CK_L();
		r <<= 1; if (DO) r++;	/* bit3 */
		CK_H(); CK_L();
		r <<= 1; if (DO) r++;	/* bit2 */
		CK_H(); CK_L();
		r <<= 1; if (DO) r++;	/* bit1 */
		CK_H(); CK_L();
		r <<= 1; if (DO) r++;	/* bit0 */
		CK_H(); CK_L();
		*buff++ = r;			/* Store a received byte */
	} while (--bc);
#endif        
}



/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static
int wait_ready (void)	/* 1:OK, 0:Timeout */
{
	BYTE d;
	UINT tmr, tmout;

	tmr = _cnt();
	tmout = _clockfreq() >> 1;  // 500 ms timeout
	for(;;) {
		rcvr_mmc( &d, 1 );
		if( d == 0xFF )  return 1;
		if( _cnt() - tmr >= tmout )  return 0;
	}
}



/*-----------------------------------------------------------------------*/
/* Deselect the card and release SPI bus                                 */
/*-----------------------------------------------------------------------*/

static
void deselect (void)
{
	BYTE d;
	int PIN_SS = _pin_ss;
	int PIN_CLK = _pin_clk;
	int PIN_DI = _pin_di;
	int PIN_DO = _pin_do;

	CS_H();				/* Set CS# high */
	rcvr_mmc(&d, 1);	/* Dummy clock (force DO hi-z for multiple slave SPI) */
}



/*-----------------------------------------------------------------------*/
/* Select the card and wait for ready                                    */
/*-----------------------------------------------------------------------*/

static
int select (void)	/* 1:OK, 0:Timeout */
{
	BYTE d;
	int PIN_SS = _pin_ss;
	
#ifdef _smartpins_mode_eh
	int PIN_DO = _pin_do;

	_pinf(PIN_DO);  // disable rx smartpin
	CS_L();			/* Set CS# low */
	_dirh(PIN_DO);  // enable rx smartpin
#else
	CS_L();			/* Set CS# low */
#endif
	rcvr_mmc(&d, 1);	/* Dummy clock (force DO enabled) */
	if (wait_ready()) return 1;	/* Wait for card ready */

	deselect();
	return 0;			/* Failed */
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from the card                                   */
/*-----------------------------------------------------------------------*/

static
int rcvr_datablock (	/* 1:OK, 0:Failed */
	BYTE *buff,			/* Data buffer to store received data */
	UINT btr			/* Byte count */
)
{
	BYTE d[2];
	UINT tmr, tmout;

	tmr = _cnt();
	tmout = _clockfreq() >> 3;  // 125 ms timeout
	for(;;) {
		rcvr_mmc( &d[0], 1 );
		if( d[0] != 0xFF )  break;
		if( _cnt() - tmr >= tmout )  break;
	}
	if (d[0] != 0xFE) return 0;		/* If not valid data token, return with error */

	rcvr_mmc(buff, btr);			/* Receive the data block into buffer */
	rcvr_mmc(d, 2);				/* Discard CRC */

	return 1;				/* Return with success */
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to the card                                        */
/*-----------------------------------------------------------------------*/

static
int xmit_datablock (	/* 1:OK, 0:Failed */
	const BYTE *buff,	/* 512 byte data block to be transmitted */
	BYTE token			/* Data/Stop token */
)
{
	BYTE d[2];


	if (!wait_ready()) return 0;

	d[0] = token;
	xmit_mmc(d, 1);				/* Xmit a token */
	if (token != 0xFD) {		/* Is it data token? */
		xmit_mmc(buff, 512);	/* Xmit the 512 byte data block to MMC */
		rcvr_mmc(d, 2);			/* Xmit dummy CRC (0xFF,0xFF) */
		rcvr_mmc(d, 1);			/* Receive data response */
		if ((d[0] & 0x1F) != 0x05)	/* If not accepted, return with error */
			return 0;
	}

	return 1;
}



/*-----------------------------------------------------------------------*/
/* Send a command packet to the card                                     */
/*-----------------------------------------------------------------------*/

static
BYTE send_cmd (		/* Returns command response (bit7==1:Send failed)*/
	BYTE cmd,		/* Command byte */
	DWORD arg		/* Argument */
)
{
	BYTE n, d, buf[6];


	if (cmd & 0x80) {	/* ACMD<n> is the command sequense of CMD55-CMD<n> */
		cmd &= 0x7F;
		n = send_cmd(CMD55, 0);
		if (n > 1) return n;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect();
		if (!select()) return 0xFF;
	}

	/* Send a command packet */
	buf[0] = 0x40 | cmd;			/* Start + Command index */
	buf[1] = (BYTE)(arg >> 24);		/* Argument[31..24] */
	buf[2] = (BYTE)(arg >> 16);		/* Argument[23..16] */
	buf[3] = (BYTE)(arg >> 8);		/* Argument[15..8] */
	buf[4] = (BYTE)arg;				/* Argument[7..0] */
	n = 0x01;						/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;		/* (valid CRC for CMD0(0)) */
	if (cmd == CMD8) n = 0x87;		/* (valid CRC for CMD8(0x1AA)) */
	buf[5] = n;
	xmit_mmc(buf, 6);

	/* Receive command response */
	if (cmd == CMD12) rcvr_mmc(&d, 1);	/* Skip a stuff byte when stop reading */
	n = 10;								/* Wait for a valid response in timeout of 10 attempts */
	do
		rcvr_mmc(&d, 1);
	while ((d & 0x80) && --n);

	return d;			/* Return with the response value */
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE drv			/* Drive number (always 0) */
)
{
	if (drv) return STA_NOINIT;

	return Stat;
}



/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE drv		/* Physical drive nmuber (0) */
)
{
	BYTE n, ty, cmd, buf[4];
	UINT tmr, ck_div, spm_ck, spm_tx, spm_rx;
	DSTATUS s;
	int PIN_SS = _pin_ss;
	int PIN_CLK = _pin_clk;
	int PIN_DI = _pin_di;
	int PIN_DO = _pin_do;

        Stat = STA_NOINIT;
        
#ifdef _DEBUG_SDMM
        __builtin_printf("disk_initialize: PINS=%d %d %d %d\n", PIN_SS, PIN_CLK, PIN_DI, PIN_DO);
#endif	
	if (drv) {
#ifdef _DEBUG_SDMM
            __builtin_printf("bad drv %d\n", drv);
#endif	    
            return RES_NOTRDY;
        }

	dly_us(10000);			/* 10ms */

#ifdef _smartpins_mode_eh
	_wrpin( PIN_SS, 0 );
	_wrpin( PIN_CLK, 0 );
	_wrpin( PIN_DI, 0 );
	_wrpin( PIN_DO, P_HIGH_15K | P_LOW_15K );
	_pinh( PIN_SS );  // Deselect SD card
	_pinh( PIN_CLK );  // CLK idles high
	_pinh( PIN_DI );  // DI idles 0xff
	_pinh( PIN_DO );  // 15 k pull-up on DO

// I/O registering (P_SYNC_IO) the SPI clock pin was found to be vital for stability.  Although it
//   adds extra lag to the tx pin, it also effectively (when rx pin is unregistered) makes a late-late
//   rx sample point.
	ck_div = 0x0008_0010;  // sysclock/16
	spm_ck = P_PULSE | P_OE | P_INVERT_OUTPUT | P_SCHMITT_A;  // CPOL = 1 (SPI mode 3)
	_pinstart( PIN_CLK, spm_ck | P_SYNC_IO, ck_div, 0 );

	spm_tx = P_SYNC_TX | P_OE | (((PIN_CLK - PIN_DI) & 7) << 24);  // tx smartpin mode and clock pin offset
	_pinstart( PIN_DI, spm_tx | P_SYNC_IO, 31, -1 );  // rising clock + 5 ticks lag, 32-bit, continuous mode, initial 0xff

	spm_rx = ((PIN_CLK - PIN_DO) & 7) << 24;  // clock pin offset for rx smartpin
	spm_rx |= P_SYNC_RX | P_OE | P_INVERT_OUTPUT | P_HIGH_15K | P_LOW_15K;  // rx smartpin mode, with 15 k pull-up
	_pinstart( PIN_DO, spm_rx, 7 | 32, 0 );  // 8-bit, sample after rising clock + 1 tick delay (smartB registration)
#else        
	CS_INIT(); CS_H();		/* Initialize port pin tied to CS */
	CK_INIT(); CK_L();		/* Initialize port pin tied to SCLK */
	DI_INIT();				/* Initialize port pin tied to DI */
	DO_INIT();				/* Initialize port pin tied to DO */
#endif
        
	rcvr_mmc(buf, 10);  // Apply 80 dummy clocks and the card gets ready to receive command
	send_cmd(CMD0, 0);  // Enter Idle state
	deselect();
	dly_us(100);

	rcvr_mmc(buf, 10);  // Apply 80 dummy clocks and the card gets ready to receive command

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
#ifdef _DEBUG_SDMM
            __builtin_printf("idle OK\n");
#endif	    
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2? */
			rcvr_mmc(buf, 4);							/* Get trailing return value of R7 resp */
			if (buf[2] == 0x01 && buf[3] == 0xAA) {		/* The card can work at vdd range of 2.7-3.6V */
				for (tmr = 1000; tmr; tmr--) {			/* Wait for leaving idle state (ACMD41 with HCS bit) */
					if (send_cmd(ACMD41, 1UL << 30) == 0) break;
					dly_us(1000);
				}
				if (tmr && send_cmd(CMD58, 0) == 0) {	/* Check CCS bit in the OCR */
					rcvr_mmc(buf, 4);
					ty = (buf[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 */
				}
#ifdef _smartpins_mode_eh
				tmr = _clockfreq();
				spm_tx |= P_INVERT_B;
				// Performance option for "Default Speed" (Up to 50 MHz SPI clock)
				if( tmr <= 150_000_000 )  ck_div = 0x0002_0004;  // sysclock/4
				else if( tmr <= 200_000_000 )  ck_div = 0x0002_0005;  // sysclock/5
				else if( tmr <= 280_000_000 )  ck_div = 0x0002_0006;  // sysclock/6
				else  ck_div = 0x0003_0008;  // sysclock/8
#endif
			}
		} else {							/* SDv1 or MMCv3 */
			if (send_cmd(ACMD41, 0) <= 1) 	{
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			for (tmr = 1000; tmr; tmr--) {			/* Wait for leaving idle state */
				if (send_cmd(cmd, 0) == 0) break;
				dly_us(1000);
			}
			if (!tmr || send_cmd(CMD16, 512) != 0)	{/* Set R/W block length to 512 */
                            //printf("tmr = %d\n", tmr);
                            ty = 0;
                        }
#ifdef _smartpins_mode_eh
			tmr = _clockfreq();
			if( tmr <= 100_000_000 )  spm_tx |= P_INVERT_B;  // falling clock + 4 tick lag + 1 tick (smartB registration)
			else if( tmr <= 200_000_000 )  spm_tx |= P_INVERT_B | P_SYNC_IO;  // falling clock + 5 tick lag + 1 tick
			// else:  spm_tx default, rising clock + 4 tick lag + 1 tick (smartB registration)
		// Reliable option (Up to 25 MHz SPI clock)
			if( tmr <= 100_000_000 )  ck_div = 0x0002_0004;  // sysclock/4
			else if( tmr <= 150_000_000 )  ck_div = 0x0003_0006;  // sysclock/6
			else if( tmr <= 200_000_000 )  ck_div = 0x0004_0008;  // sysclock/8
			else if( tmr <= 250_000_000 )  ck_div = 0x0005_000a;  // sysclock/10
			else if( tmr <= 300_000_000 )  ck_div = 0x0006_000c;  // sysclock/12
			else  ck_div = 0x0007_000e;  // sysclock/14
#endif
		}
	}
#ifdef _DEBUG_SDMM
        __builtin_printf("ty = %d\n", ty);
#endif	
	CardType = ty;
	s = ty ? 0 : STA_NOINIT;
	Stat = s;

	deselect();

#ifdef _smartpins_mode_eh
	_wxpin( PIN_CLK, ck_div );
	_wrpin( PIN_DI, spm_tx );
  #ifdef _DEBUG_SDMM
	__builtin_printf( "SPI clock ratio = sysclock/%d\n", ck_div & 0xffff );
  #endif
#endif
	return s;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE drv,			/* Physical drive nmuber (0) */
	BYTE *buff,			/* Pointer to the data buffer to store read data */
	LBA_t sector,		/* Start sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	BYTE cmd;
	DWORD sect = (DWORD)sector;

#ifdef _DEBUG
        __builtin_printf("disk_read: PINS=%d %d %d %d\n", _pin_ss, _pin_clk, _pin_di, _pin_do);
#endif	

	if (disk_status(drv) & STA_NOINIT) return RES_NOTRDY;
	if (!(CardType & CT_BLOCK)) sect *= 512;	/* Convert LBA to byte address if needed */

	cmd = count > 1 ? CMD18 : CMD17;			/*  READ_MULTIPLE_BLOCK : READ_SINGLE_BLOCK */
	if (send_cmd(cmd, sect) == 0) {
		do {
			if (!rcvr_datablock(buff, 512)) break;
			buff += 512;
		} while (--count);
		if (cmd == CMD18) send_cmd(CMD12, 0);	/* STOP_TRANSMISSION */
	}
	deselect();

	return count ? RES_ERROR : RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	BYTE drv,			/* Physical drive nmuber (0) */
	const BYTE *buff,	/* Pointer to the data to be written */
	LBA_t sector,		/* Start sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	DWORD sect = (DWORD)sector;


	if (disk_status(drv) & STA_NOINIT) return RES_NOTRDY;
	if (!(CardType & CT_BLOCK)) sect *= 512;	/* Convert LBA to byte address if needed */

	if (count == 1) {	/* Single block write */
		if ((send_cmd(CMD24, sect) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE))
			count = 0;
	}
	else {				/* Multiple block write */
		if (CardType & CT_SDC) send_cmd(ACMD23, count);
		if (send_cmd(CMD25, sect) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD))	/* STOP_TRAN token */
				count = 1;
		}
	}
	deselect();

	return count ? RES_ERROR : RES_OK;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive nmuber (0) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	BYTE n, csd[16];
	DWORD cs;

	if (disk_status(drv) & STA_NOINIT) return RES_NOTRDY;	/* Check if card is in the socket */

	res = RES_ERROR;
	switch (ctrl) {
		case CTRL_SYNC :		/* Make sure that no pending write process */
			if (select()) res = RES_OK;
			break;

		case GET_SECTOR_COUNT :	/* Get number of sectors on the disk (DWORD) */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
				if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
					cs = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
					*(LBA_t*)buff = cs << 10;
				} else {					/* SDC ver 1.XX or MMC */
					n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
					cs = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
					*(LBA_t*)buff = cs << (n - 9);
				}
				res = RES_OK;
			}
			break;

		case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (DWORD) */
			*(DWORD*)buff = 128;
			res = RES_OK;
			break;

		default:
			res = RES_PARERR;
	}

	deselect();

	return res;
}

DRESULT disk_setpins(int drv, int pclk, int pss, int pdi, int pdo)
{
    if (drv != 0) return -1;
    _pin_clk = pclk;
    _pin_ss  = pss;
    _pin_di = pdi;
    _pin_do = pdo;
#ifdef _DEBUG
    __builtin_printf("&_pin_clk=%x, _pin_clk = %d\n", (unsigned)&_pin_clk, _pin_clk);
#endif    
}
