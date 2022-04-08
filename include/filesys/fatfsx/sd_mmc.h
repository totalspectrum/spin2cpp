/**
 * @brief Interface Driver for SD cards
 * @author Michael Burmeister
 * @date Feburary 1, 2021
 * @version 1.0
 * 
*/

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

/**
 * @brief Set SD Interface
 * @param cs chip select pint
 * @param clk clock pin
 * @param mosi master out slave in
 * @param miso master in slave out
 */
void SetSD(int cs, int clk, int mosi, int miso) __fromfile("filesys/fatfsx/sd_mmc.c");

/**
 * @brief Enable SD Interface
 */
void EnableSD(void) __fromfile("filesys/fatfsx/sd_mmc.c");

/**
 * @brief Send Data to SD Memory Card
 * @param buff pointer to buffer data
 * @param bc number of bytes to send
 */
void SendSD(const char *buff, unsigned int bc);

/**
 * @brief Receive Data from SD Memory Card
 * @param buff pointer to buffer data
 * @param bc number of bytes to receive
 */
void ReceiveSD(char *buff, unsigned int bc);

/**
 * @brief Get ready state for SD Memory Card
 * @return ready ready status 0 - not ready
 */
int GetStatus(void);

/**
 * @brief Release SD Memory Card
 */
void ReleaseSD(void);

/**
 * @brief Select SD Memory Card
 * @return status 0 - not ready
 */
int SelectSD(void);

/**
 * @brief Receive Block of data from SD Memory Card
 * @param buff pointer to buffer of data
 * @param bc byte count to receive
 * @return status 0 - not ready
 */
int ReceiveBlock(char *buff, unsigned int bc);

/**
 * @brief Send Block of data to SD Memory Card
 * @param buff pointer to buffer of data
 * @param token end of send data
 * @return status 0 - not ready
 */
int SendBlock(char *buff, int token);

/**
 * @brief Send Command to SD Memory Card
 * @param cmd command to send
 * @param arg command options
 * @return status bit 7 == 1 failed
 */
char SendCommand(char cmd, unsigned int arg);

/**
 * @brief Disk Status
 * @return status
 */
char MMC_disk_status(void);

/**
 * @brief Disk Initialize
 * @return status
 */
char MMC_disk_initialize(void);

