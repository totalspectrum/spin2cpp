//
// string formatting functions
//
#ifdef __FLEXC__
typedef int (*putfunc)(int c);
#define CALL(fn, c) (*fn)(c)
#else
typedef struct {
    int (*func)(int c, void *arg);
    void *arg;
} putfunc;
#define CALL(fn, c) (*fn->func)(c, fn->arg)
#endif

//
// flags:
//  8 bits maximum width
//  8 bits minimum width
//  6 bits precision
//  2 bits padding character (0=none, 1=space, 2='0', 3=reserved)
//  2 bits justification (0=left, 1=right, 2=center)
//  2 bits sign character (0=none, 1='+', 2=' ', 3=unsigned)
//  1 bit alt (for alternate form)
//  1 bit for upper case
//
#define MAXWIDTH_BIT (0)
#define MINWIDTH_BIT (8)
#define PREC_BIT     (16)
#define JUSTIFY_BIT  (22)
#define PADDING_BIT  (24)
#define SIGNCHAR_BIT (26)
#define ALT_BIT      (28)
#define UPCASE_BIT   (29)

#define PADCHAR_NONE  0
#define PADCHAR_SPACE 1
#define PADCHAR_ZERO  2

#define SIGNCHAR_NONE 0
#define SIGNCHAR_PLUS 1
#define SIGNCHAR_SPACE 2
#define SIGNCHAR_UNSIGNED 3
#define SIGNCHAR_MINUS 3

//
// reverse a string in-place
//
void _strrev(char *str)
{
    char *end;
    int c;

    for (end = str; *end; end++) ;
    --end;
    while (end > str) {
        c = *str;
        *str++ = *end;
        *end-- = c;
    }
}

//
// add padding to left or right
// fmt == flags as above
// width == width of field to output
// leftright == 2 for left, 1 for right
//
// returns: number of bytes output
//
#define PAD_ON_LEFT 2
#define PAD_ON_RIGHT 1

int _fmtpad(putfunc fn, unsigned fmt, int width, unsigned leftright)
{
    int minwidth = (fmt & 0xff);
    unsigned justify = (fmt >> JUSTIFY_BIT) & 3;
    int i, r;
    int n = 0;
    if (justify == 0) {
        justify = 1;
    }
    if ( (justify & leftright) == 0 ) {
        return 0;  // padding goes on other side
    }
    width = minwidth - width; // (amount to pad)
    if (width <= 0) {
        return 0;
    }
    if (justify == 3) { // centering, only half as much padding (if odd do on right)
        width = (width + (leftright==PAD_ON_RIGHT)) / 2;
    }
    for (i = 0; i < width; i++) {
        r = CALL(fn, ' ');
        if (r < 0) return r;
        n += r;
    }
    return n;
}

int _fmtstr(putfunc fn, unsigned fmt, const char *str)
{
    int maxwidth = (fmt >> MAXWIDTH_BIT) & 3;
    int width = __builtin_strlen(str);
    int n;
    int r, i;
    if (maxwidth && width > maxwidth) {
        width = maxwidth;
    }
    n = _fmtpad(fn, fmt, width, PAD_ON_LEFT);
    if (n < 0) return n;
    for (i = 0; i < width; i++) {
        r = CALL(fn, *str++);
        if (r < 0) return r;
        n += r;
    }
    r = _fmtpad(fn, fmt, width, PAD_ON_RIGHT);
    if (r < 0) return r;
    n += r;
    return n;
}

int _fmtchar(putfunc fn, unsigned fmt, int c)
{
    char buf[2];
    buf[0] = c;
    buf[1] = 0;
    return _fmtstr(fn, fmt, buf);
}

int _uitoa(char *orig_str, unsigned num, unsigned base, unsigned mindigits, int uppercase)
{
    char *str = orig_str;
    unsigned digit;
    unsigned width = 0;
    int letterdigit;

    if (uppercase) {
        letterdigit = 'A' - 10;
    } else {
        letterdigit = 'a' - 10;
    }
    do {
        digit = num % base;
        if (digit < 10) {
            digit += '0';
        } else {
            digit += letterdigit;
        }
        *str++ = digit;
        num = num / base;
        width++;
    } while (num > 0 || width < mindigits);
    *str++ = 0;
    _strrev(orig_str);
    return width;
}

#define MAX_NUM_DIGITS 64

int _fmtnum(putfunc fn, unsigned fmt, int x, int base)
{
    char buf[MAX_NUM_DIGITS+1];
    char *ptr = buf;
    int width;
    int mindigits = (fmt >> PREC_BIT) & 0xff;
    int maxdigits = (fmt >> MAXWIDTH_BIT) & 0xff;
    int signchar = (fmt >> SIGNCHAR_BIT) & 0x3;
    int padchar = (fmt >> PADDING_BIT) & 0x3;
    
    if (maxdigits > MAX_NUM_DIGITS || maxdigits == 0) {
        maxdigits = MAX_NUM_DIGITS;
    }
    if (signchar == SIGNCHAR_UNSIGNED) {
        signchar = SIGNCHAR_NONE;
    } else if (x < 0) {
        signchar = SIGNCHAR_MINUS;
        x = -x;
    }
    if (padchar != PADCHAR_ZERO) {
        mindigits = 1;
    }
    if (signchar != SIGNCHAR_NONE) {
        maxdigits--;
        if (!maxdigits) {
            return _fmtchar(fn, fmt, '#');
        }
        switch (signchar) {
        case SIGNCHAR_SPACE:
            *ptr++ = ' ';
            break;
        default:
        case SIGNCHAR_PLUS:
            *ptr++ = '+';
            break;
        case SIGNCHAR_MINUS:
            *ptr++ = '-';
            break;
        }
    }
    width = _uitoa(ptr, x, base, mindigits, 0 != (fmt & (1<<UPCASE_BIT)));
    if (width > maxdigits) {
        while (maxdigits-- > 0) {
            *ptr++ = '#';
        }
        *ptr++ = 0;
    }
    return _fmtstr(fn, fmt, buf);
}

typedef int (*TxFunc)(int c);
typedef int (*RxFunc)(void);
typedef int (*CloseFunc)(void);

TxFunc _bas_tx_handles[8] = {
    &_tx, 0, 0, 0,
    0, 0, 0, 0
};
RxFunc _bas_rx_handles[8] = {
    &_rx, 0, 0, 0,
    0, 0, 0, 0
};
CloseFunc _bas_close_handles[8] = { 0 };


//
// basic interfaces
//
int _basic_open(unsigned h, TxFunc sendf, RxFunc recvf, CloseFunc closef)
{
    if (h > 7) return;
    _bas_tx_handles[h] = sendf;
    _bas_rx_handles[h] = recvf;
    _bas_close_handles[h] = closef;
    return 0;
}

void _basic_close(unsigned h)
{
    CloseFunc cf;
    if (h > 7) return;
    cf = _bas_close_handles[h];
    _basic_open(h, 0, 0, 0);
    (*cf)();
}

int _basic_print_char(unsigned h, int c, unsigned fmt)
{
    TxFunc tf = _bas_tx_handles[h];
    if (!tf) return 0;
    (*tf)(c);
    return 1;
}

int _basic_print_string(unsigned h, char *ptr, unsigned fmt)
{
    TxFunc tf = _bas_tx_handles[h];
    if (!tf) return 0;
    return _fmtstr(tf, fmt, ptr);
}

int _basic_print_unsigned(unsigned h, int x, unsigned fmt, int base)
{
    TxFunc tf = _bas_tx_handles[h];
    if (!tf) return 0;
    fmt |= 3<<SIGNCHAR_BIT;
    return _fmtnum(tf, fmt, x, base);
}

int _basic_print_integer(unsigned h, int x, unsigned fmt, int base)
{
    TxFunc tf = _bas_tx_handles[h];
    if (!tf) return 0;
    return _fmtnum(tf, fmt, x, base);
}

int _basic_get_char(unsigned h)
{
    RxFunc rf;
    if (h > 7) return -1;
    rf = _bas_rx_handles[h];
    if (!rf) return -1;
    return (*rf)();
}
