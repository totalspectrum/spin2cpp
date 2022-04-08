/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

//#define _DEBUG

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "sd_mmc.h"


static char CardType;			/* b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing */

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
     return 0;
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
    BYTE buff[4];
    BYTE cmd;
    BYTE type;
    int i;
    
    EnableSD();
    
    for (i=0;i<100;i++)
    	ReceiveSD(buff, 1);
    
    type = 0;
    if (SendCommand(CMD0, 0) == 1) //Enter Idle State
    {
        if (SendCommand(CMD8, 0x1AA) == 1) //SD v2
        {
            ReceiveSD(buff, 4);
            if ((buff[2] == 0x01)  && (buff[3] == 0xaa)) //working voltage
            {
                for (i=0;i<10000;i++) //wait to leave idle state
                {
                    if (SendCommand(ACMD41, 1 << 30) == 0)
                    	break;
                    _waitus(10);
                }
                if (i < 10000)
                {
                    if (SendCommand(CMD58, 0) == 0)
                    {
                        ReceiveSD(buff, 4);
                        if ((buff[0] & 0x40) == 0)
                        	type = CT_SDC2;
                        else
                        	type = CT_SDC2 | CT_BLOCK;
                    }
                }
            }
        }
        else
        {
            if (SendCommand(ACMD41, 0) <= 1)
            {
                type = CT_SDC1; //SDv1
                cmd = ACMD41;
            }
            else
            {
                type = CT_MMC; //MMCv3
                cmd = CMD1;
            }
            for (i=0;i<10000;i++)
            {
                if (SendCommand(cmd, 0) == 0)
                	break;
                _waitus(10);
            }
            if ((i == 10000) || (SendCommand(CMD16, 512) != 0))
            	type = 0;
        }
    }
    CardType = type;
    ReleaseSD();
#ifdef _DEBUG
    __builtin_printf("Init: %d\n", type);
#endif
    if (type == 0)
    {
        return STA_NOINIT;
    }
    return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	unsigned int count		/* Number of sectors to read */
)
{
	DRESULT res;
    BYTE cmd;
    unsigned int sect = (unsigned int)sector;
    unsigned int i;
    
    if ((CardType & CT_BLOCK) == 0)
    	sect = sect * 512;
    
    if (count > 1)
    	cmd = CMD18;
    else
    	cmd = CMD17;
    
    if (SendCommand(cmd, sect) == 0)
    {
        for (i=0;i<count;i++)
        {
            if (ReceiveBlock(buff, 512) == 0)
            	break;
            buff += 512;
        }
        if (cmd == CMD18)
        	SendCommand(CMD12, 0);
    }
    ReleaseSD();
#ifdef _DEBUG
    __builtin_printf("Reading sector(s) %d: %d\n", sect, count);
#endif

    if (i != count)
    	return RES_ERROR;
    
    return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res;
    BYTE cmd;
    DWORD sect = (DWORD)sector;
    unsigned int i;
    
    if ((CardType & CT_BLOCK) == 0)
    	sect = sect * 512;
    
    if (count == 1)
    {
        if (SendCommand(CMD24, sect) == 0)
        	if (SendBlock(buff, 0xfe) != 0)
        		i = 1;
    }
    else
    {
        if ((CardType & CT_SDC) != 0)
        	SendCommand(ACMD23, count);
        if ((SendCommand(CMD25, sect) == 0))
        {
            for (i=0;i<count;i++)
            {
                if (SendBlock(buff, 0xfc) == 0)
                	break;
                buff += 512;
            }
            if (SendBlock(0, 0xfd) == 0)
            	i = 0;
        }
    }
    ReleaseSD();
    
#ifdef _DEBUG
    __builtin_printf("Writing sector(s) %d: %d\n", sect, i);
#endif
    if (count != i)
    	return RES_ERROR;
    
    return RES_OK;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
    DRESULT res;
    BYTE csd[16];
    unsigned int cs;
    BYTE n;

    res = RES_ERROR;
    switch (ctrl)
    {
        case CTRL_SYNC:
        	if (SelectSD())
        		res = RES_OK;
        	break;
        case GET_SECTOR_COUNT:
        	if ((SendCommand(CMD9, 0) == 0))
        		if (ReceiveBlock(csd, 16) != 0)
        		{
        		    if ((csd[0] & 0x20) != 0)
        		    {
        		        cs = csd[9] + ((int)csd[8] << 8) + ((int)(csd[7] & 63) << 16) + 1;
        		        *(LBA_t*)buff = cs << 10;
        		    }
        		    else
        		    {
        		        n = (csd[5] & 0x0f) + (csd[10] >> 7) + ((csd[9] & 0x03) << 1) + 2;
        		        cs = (csd[9] >> 6) + ((short)csd[7] << 2) + ((short)(csd[6] & 0x03) << 10) + 1;
        		        *(LBA_t*)buff = cs << (n - 9);
        		    }
        		    res = RES_OK;
        		}
        	break;
        case GET_BLOCK_SIZE:
        	*(unsigned int*)buff = 128;
        	res = RES_OK;
        	break;
        default:
        	res = RES_PARERR;
    }
    ReleaseSD();
#ifdef _DEBUG
    __builtin_printf("IOCTL: ctrl:%d %d\n", ctrl, res);
#endif
    return res;
}
