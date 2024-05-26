/*
 * BASIC language I/O
 */

#include <libsys/fmt.h>

#ifndef __FLEXC__
// BASIC support routines
extern int _tx(int c);
extern int _rx(void);
#endif

typedef int (*TxFunc)(int c);
typedef int (*RxFunc)(void);
typedef int (*CloseFunc)(void);
typedef int (*VFS_CloseFunc)(vfs_file_t *);

#ifdef _SIMPLE_IO
#define _gettxfunc(h) ((void *)1)
#define _getrxfunc(h) ((void *)1)

#else
// we want the BASIC open function to work correctly with old Spin
// interfaces which don't return sensible values from send, so we
// have to wrap the send function into one which always returns 1
// use this class to do it:
// bytecode also may need wrappers for things as well
typedef struct _bas_wrap_sender {
    TxFunc ftx;
    RxFunc frx;
    CloseFunc fclose;
    int tx(int c, void *arg) { ftx(c); return 1; }
    int rx(void *arg) { return frx(); }
    int close(void *arg) { return fclose(); }
} BasicWrapper;

TxFunc _gettxfunc(unsigned h) {
    vfs_file_t *v;
    v = __getftab(h);
    if (!v || !v->state) return 0;
    return (TxFunc)&v->putchar;
}
RxFunc _getrxfunc(unsigned h) {
    vfs_file_t *v;
    v = __getftab(h);
    if (!v || !v->state) return 0;
    return (RxFunc)&v->getchar;
}
#endif

//
// basic interfaces
//

int _basic_open(unsigned h, TxFunc sendf, RxFunc recvf, CloseFunc closef)
{
#ifdef _SIMPLE_IO
    THROW_RETURN(EIO);
#else    
    struct _bas_wrap_sender *wrapper = 0;
    vfs_file_t *v;

    v = __getftab(h);
    //__builtin_printf("basic_open(%d) = %x\n", h, (unsigned)v);
    if (!v) {
        THROW_RETURN(EIO);
    }
    if (v->state) {
        _closeraw(v);
    }
    if (sendf || recvf || closef) {
        wrapper = _gc_alloc_managed(sizeof(BasicWrapper));
        if (!wrapper) {
            THROW_RETURN(ENOMEM); /* out of memory */
        }
        wrapper->ftx = 0;
        wrapper->frx = 0;
        v->vfsdata = wrapper; /* keep the garbage collector from collecting the wrapper */
    }
    if (sendf) {
        wrapper->ftx = sendf;
        v->putcf = (putcfunc_t)&wrapper->tx;
    } else {
        v->putcf = 0;
    }
    if (recvf) {
        wrapper->frx = recvf;
        v->getcf = &wrapper->rx;
    } else {
        v->getcf = 0;
    }
    if (closef) {
        wrapper->fclose = closef;
        v->close = (VFS_CloseFunc)&wrapper->close;
    } else {
        v->close = 0;
    }
    v->state = _VFS_STATE_INUSE|_VFS_STATE_RDOK|_VFS_STATE_WROK;
    return 0;
#endif    
}

int _basic_open_string(unsigned h, char *fname, unsigned iomode)
{
#ifdef _SIMPLE_IO
    THROW_RETURN(EIO);
#else    
    vfs_file_t *v;
    int r;
    
    v = __getftab(h);
    if (!v) {
        THROW_RETURN(EIO);
    }
    if (v->state) {
        _closeraw(v);
    }
    r = _openraw(v, fname, iomode, 0666);
    if (r < 0) {
        THROW_RETURN(errno);
    }
    return r;
#endif    
}

void _basic_close(unsigned h)
{
    close(h);
}

int _basic_print_char(unsigned h, int c, unsigned fmt)
{
    TxFunc fn = _gettxfunc(h);
    if (!fn) return 0;
    PUTC(fn, c);
    return 1;
}

int _basic_print_nl(unsigned h)
{
    _basic_print_char(h, 10, 0);
    return 1;
}

int _basic_print_string(unsigned h, const char *ptr, unsigned fmt)
{
    TxFunc tf = _gettxfunc(h);
    if (!tf) return 0;
    if (!ptr) return 0;
    return _fmtstr(tf, fmt, ptr);
}

int _basic_print_boolean(unsigned h, int x, unsigned fmt)
{
    const char *ptr = x ? "true" : "false";
    return _basic_print_string(h, ptr, fmt);
}

int _basic_print_unsigned(unsigned h, int x, unsigned fmt, int base)
{
    TxFunc tf = _gettxfunc(h);
    if (!tf) return 0;
    fmt |= 3<<SIGNCHAR_BIT;
    return _fmtnum(tf, fmt, x, base);
}

int _basic_print_unsigned_2(unsigned h, int x1, int x2, unsigned fmt, int base)
{
    TxFunc tf;
    int r;
    
    tf = _gettxfunc(h);
    if (!tf) return 0;
    fmt |= 3<<SIGNCHAR_BIT;
    r = _fmtnum(tf, fmt, x1, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x2, base);
    return r;
}

int _basic_print_unsigned_3(unsigned h, int x1, int x2, int x3, unsigned fmt, int base)
{
    TxFunc tf;
    int r;
    
    tf = _gettxfunc(h);
    if (!tf) return 0;
    fmt |= 3<<SIGNCHAR_BIT;
    r = _fmtnum(tf, fmt, x1, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x2, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x3, base);
    return r;
}

int _basic_print_unsigned_4(unsigned h, int x1, int x2, int x3, int x4, unsigned fmt, int base)
{
    TxFunc tf;
    int r;
    
    tf = _gettxfunc(h);
    if (!tf) return 0;
    fmt |= 3<<SIGNCHAR_BIT;
    r = _fmtnum(tf, fmt, x1, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x2, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x3, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x4, base);
    return r;
}

int _basic_print_integer(unsigned h, int x, unsigned fmt, int base)
{
    TxFunc tf;

    tf = _gettxfunc(h);
    if (!tf) return 0;
    return _fmtnum(tf, fmt, x, base);
}

int _basic_print_integer_2(unsigned h, int x1, int x2, unsigned fmt, int base)
{
    TxFunc tf;
    int r;
    
    tf = _gettxfunc(h);
    if (!tf) return 0;
    r = _fmtnum(tf, fmt, x1, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x2, base);
    return r;
}

int _basic_print_integer_3(unsigned h, int x1, int x2, int x3, unsigned fmt, int base)
{
    TxFunc tf;
    int r;
    tf = _gettxfunc(h);
    if (!tf) return 0;
    r = _fmtnum(tf, fmt, x1, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x2, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x3, base);
    return r;
}

int _basic_print_integer_4(unsigned h, int x1, int x2, int x3, int x4, unsigned fmt, int base)
{
    TxFunc tf;
    int r;
    tf = _gettxfunc(h);
    if (!tf) return 0;
    r = _fmtnum(tf, fmt, x1, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x2, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x3, base);
    r += _fmtstr(tf, fmt, ", ");
    r += _fmtnum(tf, fmt, x4, base);
    return r;
}

int _basic_print_longunsigned(unsigned h, unsigned long long x, unsigned fmt, int base)
{
    TxFunc tf = _gettxfunc(h);
    if (!tf) return 0;
    fmt |= 3<<SIGNCHAR_BIT;
    return _fmtnumlong(tf, fmt, x, base);
}

int _basic_print_longinteger(unsigned h, long long int x, unsigned fmt, int base)
{
    TxFunc tf;
    tf = _gettxfunc(h);
    if (!tf) return 0;
    return _fmtnumlong(tf, fmt, x, base);
}

int _basic_get_char(unsigned h)
{
#ifdef _SIMPLE_IO
    return _rx();
#else    
    RxFunc rf;
    rf = _getrxfunc(h);
    if (!rf) return -1;
    return (*rf)();
#endif    
}

int _basic_print_float(unsigned h, FTYPE x, unsigned fmt, int ch)
{
    TxFunc tf;
    if (fmt == 0) {
        fmt = (ch == '#') ? DEFAULT_BASIC_FLOAT_FMT : DEFAULT_FLOAT_FMT;
    }
    tf = _gettxfunc(h);
    if (!tf) return 0;
    return _fmtfloat(tf, fmt, x, ch);
}

// write "elements" items of size "size" bytes from "data"
// returns number of bytes (not elements) successfully written
int _basic_put(int h, unsigned long pos, void *data, unsigned elements, unsigned size)
{
    int r;
    unsigned bytes = elements * size;
    if (pos != 0) {
        lseek(h, pos-1, SEEK_SET);
    }
    r = write(h, data, bytes);
    if (r > 0) {
        r = r / size;
    }
    return r;
}

// read "elements" items of size "size" bytes into "data"
// returns number of bytes (not elements) successfully read
int _basic_get(int h, unsigned long pos, void *data, unsigned elements, unsigned size)
{
    int r;
    unsigned bytes = elements * size;
    if (pos != 0) {
        lseek(h, pos-1, SEEK_SET);
    }
    r = read(h, data, bytes);
    if (r > 0) {
        r = r / size;
    }
    return r;
}
