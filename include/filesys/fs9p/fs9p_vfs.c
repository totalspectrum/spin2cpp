//
// simple test program for 9p access to host files
// Written by Eric R. Smith, Total Spectrum Software Inc.
// Released into the public domain.
//

#include <stdint.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <dirent.h>

static struct __using("filesys/fs9p/fs9p_internal.cc") FS;

#define zdoGet1() _rxraw(0)

// receive an unsigned long
static unsigned int zdoGet4()
{
    unsigned r;
    r = zdoGet1();
    r = r | (zdoGet1() << 8);
    r = r | (zdoGet1() << 16);
    r = r | (zdoGet1() << 24);
    return r;
}

// send a buffer to the host
// then receive a reply
// returns the length of the reply, which is also the first
// longword in the buffer
//
// startbuf is that start of the buffer (used for both send and
// receive); endbuf is the end of data to send; maxlen is maximum
// size
static int plain_sendrecv(uint8_t *startbuf, uint8_t *endbuf, int maxlen) __attribute__(opt(no-fast-inline-asm))
{
    int len = endbuf - startbuf;
    uint8_t *buf = startbuf;
    int i = 0;
    int left;
    unsigned flags;

    if (len <= 4) {
        return -1; // not a valid message
    }

    __lockio(0);
    
    flags = _getrxtxflags();
    _setrxtxflags(0);  // raw mode
#ifdef __P2__
    *(int*)startbuf = len;
#else  
    startbuf[0] = len & 0xff;
    startbuf[1] = (len>>8) & 0xff;
    startbuf[2] = (len>>16) & 0xff;
    startbuf[3] = (len>>24) & 0xff;
#endif

    // loadp2's server looks for magic start sequence of $FF, $01
    _txraw(0xff);
    _txraw(0x01);
    while (len>0) {
        _txraw(*buf++);
        --len;
    }
#ifdef __P2__
    int c;
    __asm volatile {
        mov  buf, startbuf
        call #.zdoGet1
        mov  len, c
        call #.zdoGet1
        shl  c, #8
        or   len, c
        call #.zdoGet1
        shl  c, #16
        or   len, c
        call #.zdoGet1
        shl  c, #24
        or   len, c
        wrlong len, buf
        add  buf, #4
        mov  left, len
        sub  left, #4 wcz
 if_be  jmp  #.endit
.rdloop
        call #.zdoGet1
        wrbyte c, buf
        add  buf, #1
        djnz left, #.rdloop
        jmp  #.endit
.zdoGet1
        testp #63 wc
 if_nc  jmp   #.zdoGet1
        rdpin c, #63
  _ret_ shr   c, #24
.endit
    }
#else    
    len = zdoGet4();
    startbuf[0] = len & 0xff;
    startbuf[1] = (len>>8) & 0xff;
    startbuf[2] = (len>>16) & 0xff;
    startbuf[3] = (len>>24) & 0xff;
    buf = startbuf+4;
    left = len - 4;
    while (left > 0 && i < maxlen) {
        buf[i++] = zdoGet1();
        --left;
    }
#endif    
    _setrxtxflags(flags);

    __unlockio(0);

    return len;
}

struct vfs *
_vfs_open_host(void)
{
    int r;
    struct vfs *v;

#ifdef _DEBUG
    __builtin_printf("vfs_open_host called\n");
#endif    
    r = FS.fs_init(&plain_sendrecv);
    if (r != 0) {
#ifdef _DEBUG
       __builtin_printf("fs_init failed: result=[%d]\n", r);
       _waitms(1000);
#endif      
       _seterror(-r);
       return 0;
    }
    v = FS.get_vfs();
#ifdef _DEBUG
    __builtin_printf("_vfs_open_host: returning %x\n", (unsigned)v);
    __builtin_printf("v->open == %x\n", (unsigned)(v->open));
#endif    
    return v;
}
