//
// guts of a new printf function
// Written by Eric R. Smith (ersmith@totalspectrum.ca)
// and placed in the public domain
//

//
// supports all of the C99 standard, AFAIK
// but can be configured smaller
//

#define FULL_PRINTF   /* if not defined, do just C89 printf */
#define FLOAT_SUPPORT /* support for FLOAT */
//#define SMALL_INT   /* only support 32 bit integers */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>
#include <math.h>
#ifdef WIN32
#include <malloc.h>
#define wchar_int int
#else
#include <alloca.h>
#define wchar_int wchar_t
#endif
#include "util.h"

#ifdef SMALL_INT
#define UITYPE uint32_t
#define ITYPE  int32_t
#define ITOA_PREC   itoa_prec
#else
#define UITYPE uint64_t
#define ITYPE  int64_t
#define ITOA_PREC   lltoa_prec
#endif
#define BITCOUNT (8*sizeof(UITYPE))

static int
hibit(UITYPE x)
{
    return (x >> (BITCOUNT-1));
}

//
// get an integer out of a va_ptr list
//

static UITYPE
fetchint( va_ptr args, int hassign, int size )
{
    union xval {
        UITYPE ui;
        ITYPE i;
    } x;
    int shift;

    shift = 8 * (sizeof(UITYPE) - size);
    if (size == sizeof(long long)) {
        x.i = va_ptrarg(args, long long);
    } else {
        x.i = va_ptrarg(args, int);
    }
    if (hassign) {
        x.i = (x.i<<shift) >> shift;
        return x.i;
    } else {
        x.ui = (x.ui<<shift) >> shift;
        return x.ui;
    }
}

//
// helper functions for formatting
//
static int _fmtputc( int c, Printf_info *pi )
{
    c &= 0xff;
    pi->byteswritten++;
    return (*pi->putchar)(c, pi->putarg);
}

//
//output any required padding to force right justification
// "len" is the length of our content
//
static int _fmtpad( Printf_info *pi, int len, int onleft )
{
    int padchar = onleft ? pi->pad : ' ';
    int wide = pi->width;
    int padcnt;
    int q;

    // if content is left justified, no padding on the left
    if (pi->left && onleft) {
        return 0;
    }
    // similarly no padding on right if right justified
    if (!pi->left && !onleft) {
        return 0;
    }

    padcnt = wide - len;

    while (padcnt > 0) {
        q = _fmtputc(padchar, pi);
        if (q < 0) return q;
        --padcnt;
    }
    return 0;
}
#define _fmtpadonleft(pi, len) _fmtpad(pi, len, 1)
#define _fmtpadonright(pi, len) _fmtpad(pi, len, 0)

// output a string, possibly padding and/or limiting it
static int _fmtputstr( const char *str, Printf_info *pi )
{
    int q;
    int c;
    int len;
    int prec = pi->prec;
    int i;

    len = strlen(str);
    if (prec >= 0 && len > prec) len = prec;

    q = _fmtpadonleft(pi, len);
    if (q < 0) return q;
    for (i = 0; i< len; i++) {
        c = *str++;
        q = _fmtputc(c, pi);
        if (q < 0) return q;
    }
    q = _fmtpadonright(pi, len);
    return q;
}

// output a string, possibly padding and/or limiting it
static int _fmtputwstr( const wchar_t *wstr, Printf_info *pi )
{
    int q, r;
    wchar_t c;
    const wchar_t *ws;
    int len;
    int left;
    int prec = pi->prec;
    int j;
    char buf[16]; // big enough to hold any multi-byte sequence
    mbstate_t MB;

    memset(&MB, 0, sizeof(MB));

    // calculate number of bytes needed
    len = 0;
    for (ws = wstr; *ws; ws++) {
        q = wcrtomb( buf, *ws, &MB );
        len += (q < 0) ? 1 : q;
    }
    if (prec >= 0 && len > prec) len = prec;

    q = _fmtpadonleft(pi, len);
    if (q < 0) return q;
    left = len;
    while (left > 0) {
        c = *wstr++;
        if (!c) break;
        // convert to multibyte characters
        q = wcrtomb(buf, c, &MB);
        if (q < 0) {
            // c is not representable
            // punt
            buf[0] = '?';
            q = 1;
        }
        // check to make sure the whole multibyte sequence will fit
        left -= q;
        if (left < 0) break;
        // output the sequence
        for (j = 0; j < q; j++) {
            r = _fmtputc(buf[j], pi);
            if (r < 0) return r;
        }
    }
    q = _fmtpadonright(pi, len);
    return q;
}

//
// return an error
//
static int _fmt_unsupported( Printf_info *pi, va_ptr args )
{
    (void)fetchint(args, 0, sizeof(int));
    pi->prec = -1;
    return _fmtputstr("unknown format specifier", pi);
}

//
// process the %n spec
//
int _fmt_n( Printf_info *pi, va_ptr args )
{
    switch(pi->size) {
    case 1:
    {
        uint8_t *ptr;
        ptr = va_ptrarg( args, uint8_t* );
        *ptr = pi->byteswritten;
        break;
    }
    case 2:
    {
        uint16_t *ptr;
        ptr = va_ptrarg( args, uint16_t* );
        *ptr = pi->byteswritten;
        break;
    }
    case 4:
    {
        uint32_t *ptr;
        ptr = va_ptrarg( args, uint32_t* );
        *ptr = pi->byteswritten;
        break;
    }
    default:
    {
        uint64_t *ptr;
        ptr = va_ptrarg( args, uint64_t* );
        *ptr = pi->byteswritten;
        break;
    }
    }
    return 0;
}

//
// helper function to format an integer 
//
static int _fmtinteger( Printf_info *pi, va_ptr args, int base, int isSigned )
{
    char *digits;
    char *buf;
    int prefix = 0;
    int prefixlen = 1;
    int needupper = (pi->spec == 'X');
    int bufsize = 32;
    UITYPE ui;

    if (pi->size > sizeof(UITYPE)) {
        return _fmt_unsupported( pi, args );
    }
    ui = fetchint( args, isSigned, pi->size );
    if (pi->prec > bufsize)
        bufsize = pi->prec;

    digits = buf = (char *)alloca(bufsize+3);

    //
    // the C standard explicitly says that for octal a precision
    // of 0 should still result in a 0 being printed
    //
    if (isSigned) {
        if ( hibit(ui) ) {
            ui = -ui;
            prefix = '-';
        } else if (pi->showsign) {
            prefix = '+';
        } else if (pi->space) {
            prefix = ' ';
        }
    } else if (pi->alt && base != 10) {
        if (ui != 0 || (base == 8 && pi->prec == 0) ) {
            prefix = '0';
        }
    }
    if (prefix) {
        *digits++ = prefix;
        if (base == 16) {
            *digits++ = 'x';
            prefixlen++;
        }
        if (pi->pad == '0') {
            pi->pad = ' ';
            if (pi->prec < 0) {
                pi->prec = pi->width - prefixlen;
            }
        }
    }

    ITOA_PREC(ui, digits, base, pi->prec);
    pi->prec = -1;
    pi->longflag = 0;
    if (needupper) {
        strupr(buf);
    }
    return _fmtputstr( buf, pi );
}

static int _fmt_decimal( Printf_info *pi, va_ptr args )
{
    int isSigned = !(pi->spec == 'u');
    return _fmtinteger(pi, args, 10, isSigned);
}

static int _fmt_hex( Printf_info *pi, va_ptr args)
{
    return _fmtinteger(pi, args, 16, 0);
}

static int _fmt_octal( Printf_info *pi, va_ptr args)
{
    return _fmtinteger(pi, args, 8, 0);
}

static int parseint( const char **fmt_p, va_ptr args )
{
    const char *fmt = *fmt_p;
    int r = 0;
    int c = *fmt;

    if (c == '*') {
        r = va_ptrarg( args, int );
        fmt++;
    } else {
        while ( (c>='0') && (c<='9') ) {
            r = 10*r + (c-'0');
            c = *++fmt;
        }
    }
    *fmt_p = fmt;
    return r;
}

// parse the printf flags (like 0, -, etc)
// also initializes the width and prec fields
static const char *parseflags(const char *fmt, Printf_info *pi)
{
    int c;
    pi->width = 0;
    pi->prec = -1;
    pi->pad = ' ';
    pi->left = pi->alt = pi->showsign = pi->space = 0;
    pi->longflag = 0;

    for(;;) {
        c = *fmt++;
        switch (c) {
        case '-':
            pi->left = 1;
            break;
        case '#':
            pi->alt = 1;
            break;
        case '+':
            pi->showsign = 1;
            break;
        case '0':
            pi->pad = '0';
            break;
        case ' ':
            pi->space = 1;
            break;
        default:
            return fmt-1;
        }
    }   
}

// parse the flag indicators ('l', 'j', etc.
static const char *parsesize(const char *fmt, Printf_info *pi)
{
    int c;
    pi->size = sizeof(int);
    c = *fmt++;

    switch (c) {
    case 'l':
        pi->size = sizeof(long);
        pi->longflag = 1;
        if (*fmt == 'l') {
            pi->size = sizeof(long long);
            fmt++;
        }
        break;
    case 'h':
        pi->size = sizeof(short);
        if (*fmt == 'h') {
            pi->size = sizeof(char);
            fmt++;
        }
        break;
#ifdef FULL_PRINTF
    case 'j':
        pi->size = sizeof(uintmax_t);
        break;
    case 'z':
    case 't':  // ASSUMPTION: sizeof(ptrdiff_t) == sizeof(size_t)
        pi->size = sizeof(size_t);
        break;
    case 'L':
        pi->size = sizeof(long double);
        pi->longflag = 1;
        break;
#endif
    default:
        // we incremented one too far
        --fmt;
        break;
    }
    return fmt;
}

static int _fmt_percent(Printf_info *pi, va_ptr args)
{
    return _fmtputc('%', pi);
}

static int _fmt_ptr(Printf_info *pi, va_ptr args)
{
    pi->size = sizeof(void *);
    pi->alt = 1; // force 0x prefix
    return _fmt_hex(pi, args);
}

static int _fmt_wstring(Printf_info *pi, va_ptr args)
{
    wchar_t *ptr;
    pi->pad = ' ';
    ptr = va_ptrarg(args, wchar_t *);
    return _fmtputwstr(ptr, pi);
}

static int _fmt_wchar(Printf_info *pi, va_ptr args)
{
    wchar_t ptr[2];
    pi->pad = ' ';
    ptr[0] = va_ptrarg(args, wchar_int);
    ptr[1] = 0;
    return _fmtputwstr(ptr, pi);
}

static int _fmt_char(Printf_info *pi, va_ptr args)
{
    char buf[2];
    if (pi->longflag)
        return _fmt_wchar(pi, args);
    pi->pad = ' ';
    buf[0] = va_ptrarg(args, int);
    buf[1] = 0;
    return _fmtputstr(buf, pi);
}

static int _fmt_string(Printf_info *pi, va_ptr args)
{
    char *ptr;
    if (pi->longflag)
        return _fmt_wstring(pi, args);
    pi->pad = ' ';
    ptr = va_ptrarg(args, char *);
    return _fmtputstr(ptr, pi);
}

#ifdef FLOAT_SUPPORT
/*******************************************************
 * floating point support
 *******************************************************/
#ifdef __propeller__
typedef long double double64;
#else
typedef double double64;
#endif

#define DOUBLE_BITS 60
#define DOUBLE_ONE (1ULL<<DOUBLE_BITS)
#define DOUBLE_MASK (DOUBLE_ONE-1)

typedef union DI {
    double64 d;
    uint64_t i;
} DI;

struct hifloat {
    uint64_t ai;
    int      exp;
    int      reserved_for_flags;
};
#ifdef __propeller__
// built in to the propeller's ASM library
extern double64 _intpowfx(double64 a, double64 b, int n, struct hifloat *p);
#else
//
// _intpowfx: calcuate a * b^n with as much precision as possible
// also returns a broken down structure with hf.ai == mantissa, hf.exp == exponent
//
double64 _intpowfx(double64 a, double64 b, int n, struct hifloat *hf)
{
    long double p10 = powl((long double)b, (long double)n);
    long double r = p10 * (long double)a;
    long double mant;

    if (a == 0.0) {
        hf->ai = 0;
        hf->exp = 0;
        return 0.0;
    }
    p10 = powl((long double)b, (long double)n);
    r = p10 * (long double)a;
    mant = frexpl(r, &hf->exp);
    hf->ai = (uint64_t)(mant * DOUBLE_ONE);
    return (double)r;
}

#endif

/*
 * disassemble a positive floating point number x into
 * ai,n such that 
 * 1.0 <= a < base and x=a * base^n
 * (normally base will be 10, but we can accept 2 as well)
 * we output ai as an integer, but conceptually it is still
 * a float with one digit before the decimal point
 * if numdigits is negative, then make numdigits be n-(numdigits+1)
 * so as to get that many digits after the decimal point
 */
static void disassemble(double64 x, uint64_t *aip, int *np, int numdigits, int base)
{
    double64 a;
    int maxdigits;
    double64 based = (double64)base;
    uint64_t ai;
    uint64_t u;
    uint64_t maxu;
    int n;
    int trys;
    struct hifloat hf;

    if (x == 0.0) {
        *aip = 0;
        *np = 0;
        return;
    }

    // first, find (a,n) such that
    // 1.0 <= a < base and x = a * base^n
    if (sizeof(x) == sizeof(double)) {
        n = ilogb(x);
    } else {
        n = ilogbl(x);
    }
    if (base == 10) {
        // initial estimate: 2^10 ~= 10^3, so 3/10 of ilogb is a good first guess
        n = (3 * n)/10 ;
        maxdigits = 17;
    } else {
        maxdigits = 53;  // for base 2
    }
    
    for (trys = 0; trys < 8; trys++) {
        //
        //
        a = _intpowfx(x, based, -n, &hf);
        if (a < 1.0) {
            --n;
        } else if (a >= based) {
            ++n;
        } else {
            break;
        }
    }
#ifdef TEST
    if (trys == 8) {
        fprintf(stderr, "Warning hit retry count\n");
    }
#endif
    if (hf.exp < 0) {
        ai = hf.ai >> -hf.exp;
    } else {
        ai = hf.ai << hf.exp;
    }
    // base 2 we will group digits into 4 to print as hex
    if (base == 2) numdigits *= 4;

    // now extract as many significant digits as we can
    // into u
    u = 0;
    if (numdigits< 0) {
        numdigits = n - numdigits;

        // "0" digits is a special case (we may need to round the
        // implicit 0 up to 1)
        // negative digits will always mean 0 though
        if (numdigits < 0) {
            goto done;
        }
    } else {
        numdigits = numdigits+1;
    }
    if (numdigits > maxdigits)
        numdigits = maxdigits;
    maxu = 1; // for overflow
    while ( u < DOUBLE_ONE && numdigits-- > 0) {
        uint64_t d;
        d = (ai >> DOUBLE_BITS); // next digit
        ai &= DOUBLE_MASK;
        u = u * base;
        maxu = maxu * base;
        ai = ai * base;
        u = u+d;
    }
    //
    // round
    //
    if (ai > (base*DOUBLE_ONE/2) || (ai == (base*DOUBLE_ONE/2) && (u & 1))) {
        u++;
        if (u == maxu) {
            ++n;
        }
    }
done:
    *aip = u;
    *np = n;
}
//
// output the sign and any hex digits required
// if buf is NULL print using pi
// otherwise put into buf
// returns number of bytes output, or -1 if an error happens
//
static int
emitsign(Printf_info *pi, char *buf, int sign, int hex)
{
    int r = 0;
    if (sign) {
        if (buf) {
            *buf++ = sign;
        } else {
            if (_fmtputc(sign, pi) < 0) return -1;
        }
        r++;
    }
    if (hex) {
        if (buf) {
            *buf++ = '0';
            *buf++ = hex;
        } else {
            if (_fmtputc('0', pi) < 0) return -1;
            if (_fmtputc(hex, pi) < 0) return -1;
        }
        r += 2;
    }
    return r;
}


//
// find the digits for x 
// we will output "prec" digits after the decimal point
//
static int
_fmt_float(Printf_info *pi, va_ptr args)
{
    double x;
    uint64_t ai;
    int i;
    int base = 10;
    int exp;  // exponent
    int prec;
    int isExpFmt;  // output should be printed in exponential notation
    int isGFmt;    // format character was %g
    int totalWidth;
    int sign = 0;
    int needUpper = 0;
    int expchar = 'e';
    int stripTrailingZeros = 0;
    int expprec = 2;
    int needPrefix = 0;
    int hexSign = 0;

    // we print a series of 0's, then the digits, then more 0's if necessary
    // to force e.g. 4 leading 0's, set startdigit to -4
    int startdigit;
    int decpt;
    int enddigit;
    int numdigits;
    int expdigits;
    int expsign = 0;
    char dig[64]; // digits for the number
    char expdig[8]; // digits for the exponent

    char *buf;
    char *origbuf;

    x = va_ptrarg(args,  double);
    prec = pi->prec;

    // check various format stuff
    if (isupper(pi->spec)) {
        needUpper = 1;
    }
    isExpFmt = tolower(pi->spec) == 'e';
    isGFmt = tolower(pi->spec) == 'g';
    if (tolower(pi->spec) == 'a') {
        isExpFmt = 1;
        expchar = 'p';
        base = 2;
        // if precision is missing, %a needs enough digits
        // to precisely represent the number
        if (prec < 0) {
            prec = 13;
            stripTrailingZeros = 1;
        }
        expprec = 1;
        hexSign = needUpper ? 'X' : 'x';
    } else {
        // default precision
        if (prec < 0) {
            prec = 6;
        }
    }
    pi->prec = -1; // so we can print arbitrarily long strings with pi

    if ( signbit(x) ) {
        // work on non-negative values only
        sign = '-';
        x = copysign(x, 1.0);
    } else {
        // sometimes we need positive sign
        if (pi->showsign) {
            sign = '+';
        } else if (pi->space) {
            sign = ' ';
        }
    }

    // if user requested 0 padding, emit the sign and adjust width
    // now
    needPrefix = (sign || base != 10);
    if ( needPrefix && pi->pad == '0' && !pi->left) {
        int r;
        r = emitsign(pi, NULL, sign, hexSign);
        if (r < 0) return r;
        pi->width -= r;
        needPrefix = 0;
    }

    // handle special cases
    if (isinf(x)) {
        origbuf = buf = (char *)alloca(16);
        // emit sign
        if (sign) *buf++ = sign;
        strcpy(buf, "inf");
        goto done;
    } else if (isnan(x)) {
        origbuf = buf = (char *)alloca(6);
        if (sign) *buf++ = sign;
        strcpy(buf, "nan");
        goto done;
    }

    if (isGFmt) {
        // find the exponent
        disassemble(x, &ai, &exp, prec ? prec - 1 : 0, base);
        // for g format, special handling
        stripTrailingZeros = !(pi->alt);
        if (exp >= prec || exp < -4) {
            isExpFmt = 1;
        } else {
            prec = prec - exp;
            disassemble(x, &ai, &exp, -prec, base);
        }
    } else if (isExpFmt) {
        disassemble(x, &ai, &exp, prec, base);
    } else {
        disassemble(x, &ai, &exp, -(prec+1), base);
    }

    //
    // figure out how many digits we have to print
    //

    // put the digits in their buffers
    // now, a satisfies ai * 10^exp == x
    // if base 2, print digits in hex
    if (base == 2) {
        base = 16;
    }

    numdigits = ITOA_PREC(ai, dig, base, 1);
    if (exp < 0) {
        expsign = '-';
        expdigits = ITOA_PREC(-exp, expdig, 10, expprec);
    } else {
        expsign = '+';
        expdigits = ITOA_PREC(exp, expdig, 10, expprec);
    }

    if (isExpFmt) {
        startdigit = decpt = 0;
        enddigit = prec + 1;
    } else {
        if (exp < 0) {
            // force leading 0's
            startdigit = decpt = exp;
            enddigit = exp + prec + 1;
        } else {
            startdigit = 0;
            decpt = exp;
            enddigit = decpt + prec + 1;
        }
    }

    // allocate a string buffer; leave extra room for '.' just in case
    totalWidth = (enddigit - startdigit) + 1;
    if (sign) {
        totalWidth++;  // we will be printing '+' or '-'
    }
    if (base == 16) {
        totalWidth += 2;  // for '0x'
    }
    if (isExpFmt) {
        // exponents can go up to +- 1024
        totalWidth += 2 + expdigits; // (for e+1024)
    }

    origbuf = buf = (char *)alloca(totalWidth + 1);

    // emit sign
    if (needPrefix) {
        int r = emitsign(pi, buf, sign, hexSign);
        if (r < 0) return r;
        buf += r;
    }
    //
    //
    // emit the number
    //
    for (i = startdigit; i < enddigit; i++) {
        if (i >= 0 && i < numdigits) {
            *buf++ = dig[i];
        } else {
            *buf++ = '0';
        }
        if (i == decpt) {
            *buf++ = '.';
        }
    }

    if (stripTrailingZeros) {
        --buf;
        // remove any trailing 0's
        while (buf > origbuf && *buf == '0') {
            --buf;
        }
        // remove radix if appropriate
        if (*buf == '.') {
            --buf;
        }
        // *buf now points at the last valid character; advance so
        // that we can write starting after that
        buf++;
    }

    //
    // output the exponent
    // we want at least 2 digits
    // (up to 4 may be required)
    //
    if (isExpFmt)
    {
        *buf++ = expchar;  // 'p' for hex, 'e' for decimal
        *buf++ = expsign;
        for (i = 0; i < expdigits; i++) {
            *buf++ = expdig[i];
        }
    }

    *buf = 0;

done:
    // convert to upper case if required
    if (needUpper) {
        strupr(origbuf);
    }
    // now output the string
    return _fmtputstr(origbuf, pi);
}

#endif

/*
 * table of supported formats
 */
/* function passed to format */
typedef int (*_Fmt_func)(Printf_info *, va_list *);

struct fmtspecs {
    char spec;      // character after %
    _Fmt_func func;  // function to call
};

static struct fmtspecs fmtspecs[] = {
    { 'd', _fmt_decimal },
    { 'i', _fmt_decimal },
    { 'u', _fmt_decimal },
    { 'x', _fmt_hex },
    { 'X', _fmt_hex },
    { 'p', _fmt_ptr },
    { 's', _fmt_string },
    { 'c', _fmt_char },
    { '%', _fmt_percent },
    { 'o', _fmt_octal },
    { 'C', _fmt_wchar },
    { 'S', _fmt_wstring },
#ifdef FULL_PRINTF
    { 'n', _fmt_n },
#endif
#ifdef FLOAT_SUPPORT
    { 'e', _fmt_float },
    { 'E', _fmt_float },
    { 'f', _fmt_float },
    { 'F', _fmt_float },
    { 'g', _fmt_float },
    { 'G', _fmt_float },
    { 'a', _fmt_float },
    { 'A', _fmt_float },
#endif
    { 0, 0 }
};

int
dofmt( FmtPutchar func, void *funcarg, const char *fmt, va_ptr args )
{
    int c;
    int q;
    Printf_info pi;
    int i;

    memset(&pi, 0, sizeof(pi));
    pi.putarg = funcarg;
    pi.putchar = func;

    for(;;) {
        c = *fmt++;
        if (c == 0) break;
        if (c != '%') {
            q = _fmtputc(c, &pi);
            if (q < 0) return q;
            continue;
        }
        fmt = parseflags(fmt, &pi);
        
        pi.width = parseint(&fmt, args);
        if (pi.width < 0) {
            pi.width = -pi.width;
            pi.left = 1;
        }
        c = *fmt; if (c == 0) break;
        if (c == '.') {
            fmt++;
            pi.prec = parseint(&fmt, args);
            c = *fmt; if (c == 0) break;
        }
        fmt = parsesize(fmt, &pi);
        c = *fmt++; if (c == 0) break;
        pi.spec = c;
        q = 0;
        for (i = 0; fmtspecs[i].func; i++) {
            if (fmtspecs[i].spec == c) {
                q = (*fmtspecs[i].func)(&pi, args);
                break;
            }
        }
        if (fmtspecs[i].func == NULL) {
            q = _fmt_unsupported(&pi, args);
        }
        if (q < 0) return q;
    }
    return pi.byteswritten;
}

#ifdef TEST
struct sprintf_info {
    char *ptr;
    char *end;
};

static int sputc(int c, void *arg)
{
    struct sprintf_info *S = (struct sprintf_info *)arg;
    if (S->end && S->ptr == S->end) return -1;
    *S->ptr++ = c;
    return 0;
}

int
mysnprintf( char *s, size_t len, const char *fmt, ...)
{
    va_list args;
    int r;
    struct sprintf_info S;

    S.ptr = s;
    S.end = s + len;

    va_start(args, fmt);
    r = _dofmt( sputc, &S, fmt, &args );
    va_end(args);
    sputc(0, &S );
    return r;
}

#include <locale.h>

int fail = 0;

#define TESTSIZE 512

//
// print a string, translating non-ascii letters
//
void reportstr(const char *msg)
{
    int c;
    putchar('[');
    while ( (c = *msg++) != 0 ) {
        c &= 255;
        if (c < 127 && c >= 32) {
            putchar(c);
        } else {
            printf("\\x%02x", c);
        }
    }
    putchar(']');
}

void tests(const char* fmt, const char *arg)
{
    char expect[TESTSIZE];
    char result[TESTSIZE];
    int r1, r2;

    expect[0] = result[0] = 0;
    r1 = snprintf(expect, TESTSIZE, fmt, arg);
    r2 = mysnprintf(result, TESTSIZE, fmt, arg);
    if (0 != strcmp(expect, result)) {
        printf("printf: fmt=");
        reportstr(fmt);
        printf(" arg=");
        reportstr(arg);
        printf(" output was ");
        reportstr(result);
        printf(" expected ");
        reportstr(expect);
        printf("\n");
        fail++;
        return;
    }
    if (r1 != r2) {
        printf("printf(%s,%s): return was %d expected %d\n", fmt, arg, r2, r1);
        fail++;
        return;
    }
}

//
// test a single format strint with an integer or char input
//
void testc(const char *fmt, unsigned int x)
{
    char expect[TESTSIZE];
    char result[TESTSIZE];
    int r1, r2;

    expect[0] = result[0] = 0;
    r1 = snprintf(expect, TESTSIZE, fmt, x);
    r2 = mysnprintf(result, TESTSIZE, fmt, x);
    if (0 != strcmp(expect, result)) {
        printf("printf(%s,%u): output was ", fmt, x);
        reportstr(result);
        printf(" expected ");
        reportstr(expect);
        printf("\n");
        fail++;
        return;
    }
    if (r1 != r2) {
        printf("printf(%s,%u): return was %d expected %d\n", fmt, x, r2, r1);
        fail++;
    }
}

//
// test a format string and one arg against all the integer formats
// we do this by replacing a ~ in the format with each of d, i, u, x, etc.
//
void testi(const char* fmtorig, unsigned int x)
{
    char fmt[TESTSIZE];
    int i, j, c;
    const char intstr[] = "diuxXo";

    for (j = 0; intstr[j] != 0; j++) {
        for (i = 0; fmtorig[i] != 0; i++) {
            c = fmtorig[i];
            fmt[i] = (c == '~') ? intstr[j] : c;
        }
        fmt[i] = 0;
        testc(fmt, x);
    }
}
//
// test a format string and one arg against all the integer formats
// we do this by replacing a ~ in the format with each of d, i, u, x, etc.
//
void testll(const char* fmtorig, unsigned long long x)
{
    char fmt[TESTSIZE];
    char expect[TESTSIZE];
    char result[TESTSIZE];
    int r1, r2;
    int i, j, c;
    const char intstr[] = "diuxXo";

    for (j = 0; intstr[j] != 0; j++) {
        for (i = 0; fmtorig[i] != 0; i++) {
            c = fmtorig[i];
            fmt[i] = (c == '~') ? intstr[j] : c;
        }
        fmt[i] = 0;

        expect[0] = result[0] = 0;
        r1 = snprintf(expect, TESTSIZE, fmt, x);
        r2 = mysnprintf(result, TESTSIZE, fmt, x);
        if (0 != strcmp(expect, result)) {
            printf("printf(%s,%#llx): output was [%s] expected [%s]\n", fmt, x, result, expect);
            fail++;
            continue;
        }
        if (r1 != r2) {
            printf("printf(%s,%#llx): return was %d expected %d\n", fmt, x, r2, r1);
            fail++;
            continue;
        }
    }
}

void testi2(const char* fmtorig, unsigned int x, unsigned int y)
{
    char expect[TESTSIZE];
    char result[TESTSIZE];
    char fmt[TESTSIZE];
    int i, j, c;
    const char intstr[] = "diux";
    int r1, r2;


    for (j = 0; intstr[j] != 0; j++) {
        for (i = 0; fmtorig[i] != 0; i++) {
            c = fmtorig[i];
            fmt[i] = (c == '~') ? intstr[j] : c;
        }
        fmt[i] = 0;

        expect[0] = result[0] = 0;
        r1 = snprintf(expect, TESTSIZE, fmt, x, y);
        r2 = mysnprintf(result, TESTSIZE, fmt, x, y);
        if (0 != strcmp(expect, result)) {
            printf("printf(%s,%u,%u): output was [%s] expected [%s]\n", fmt, x, y, result, expect);
            fail++;
            continue;
        }
        if (r1 != r2) {
            printf("printf(%s,%u,%u): return was %d expected %d\n", fmt, x, y, r2, r1);
            fail++;
            continue;
        }
    }
}

//
// test a single format strint with a double arg
//
void testdouble(const char *fmt, double x)
{
    char expect[TESTSIZE];
    char result[TESTSIZE];
    int r1, r2;

    expect[0] = result[0] = 0;
    r1 = snprintf(expect, TESTSIZE, fmt, x);
    r2 = mysnprintf(result, TESTSIZE, fmt, x);
    if (0 != strcmp(expect, result)) {
        printf("printf(%s,%e): output was ", fmt, x);
        reportstr(result);
        printf(" expected ");
        reportstr(expect);
        printf("\n");
        fail++;
        return;
    }
    if (r1 != r2) {
        printf("printf(%s,%e): return was %d expected %d\n", fmt, x, r2, r1);
        fail++;
    }
}

//
// test a format string and one arg against all the double formats
// we do this by replacing a ~ in the format with each of e, f, g, etc.
//
void testf(const char* fmtorig, double x)
{
    char fmt[TESTSIZE];
    int i, j, c;
    const char intstr[] = "eEfFgGaA";

    for (j = 0; intstr[j] != 0; j++) {
        for (i = 0; fmtorig[i] != 0; i++) {
            c = fmtorig[i];
            fmt[i] = (c == '~') ? intstr[j] : c;
        }
        fmt[i] = 0;
        testdouble(fmt, x);
    }
}

//
// test a single format string with a uintmax_t type
//
void testj(const char *fmt, uintmax_t x)
{
    char expect[TESTSIZE];
    char result[TESTSIZE];
    int r1, r2;

    expect[0] = result[0] = 0;
    r1 = snprintf(expect, TESTSIZE, fmt, x);
    r2 = mysnprintf(result, TESTSIZE, fmt, x);
    if (0 != strcmp(expect, result)) {
        printf("printf(%s,0x%jx): output was ", fmt, x);
        reportstr(result);
        printf(" expected ");
        reportstr(expect);
        printf("\n");
        fail++;
        return;
    }
    if (r1 != r2) {
        printf("printf(%s,0x%jx): return was %d expected %d\n", fmt, x, r2, r1);
        fail++;
    }
}

int
main()
{
    char buf[128];
    wchar_t alpha[] = L"\x03b1\x03b2\x03b3";

    setlocale(LC_ALL, "");

    testj("%jx", 0x123456789abLL);

    tests("%S", (char *)alpha);
    tests("%.4ls", (char *)alpha);
    tests("%.3ls", (char *)alpha);
    testc("%c", 'a');
    testc("%-3c", 'b');
    testc("%03c", 'c');
    testc("%03C", '%');
    testc("%03C", 0x3b1); // greek alpha; utf-8 0xce 0xb1

    testi("%4hh~", 257);
    testi("%4hh~", 255);
    testi("%2h~", 65535);
    testi("%-2h~", 65537);
    testi("%2l~", 65537);

    testi("%~", 1);
    testi("%~", -1);
    testi("%4~", -2);
    testi("%04~", -3);
    testi("%.4~", -10);
    testi("%+.4~", 10);
    testi("% .4~", 99);
    testi("%+ 04~", 100);
    testi("%4~", -999);
    testi("%4~", -99);
    testi("%+4~", 99);
    testi("%04~", 999);

    testi("%~", 0);
    testi("%.0~", 0);
    testi("%.~", 0);
    testi("%#.0~", 0);
    testi("%#.3~", 0);
    testi("%9~", 33);
    testi("%#05~", 32);
    testi("%-2.0~", 0);
    testi("%#-2.0~", 0);
    testi("%.3~", 99);
    testi("%#.3~", 177);
    testi("%.3~", 1234);
    testi("%.8~", 65535);
    testi("%12.8~", 65536);
    testi("%256.100~", 199);
    testi("%#-3.0~", 10);
    testi("%-7.0~", 10);
    testi("%-1.3~", 19);
    testi("%+4~", 19);

    testll("%ll~", 17);
    testll("%ll~", -1);
    testll("%ll~", 0x100000000LL);
    testll("%ll~", -0x300000000LL);

    tests("%-8s", "xyz");
    tests("hello", "dummy");
    tests("hello%%", "dummy");
    tests("%s", "");
    tests("%s", "dummy");
    tests("%05s", "xy");
    tests("%5s", "x");
    tests("%#5s", "x");
    tests("%1s", "xyz");
    tests("%3.2s", "xyz");
    tests("%3.s", "xyz");
    tests("%.12s", "123456789abcdefghijk");
    tests("%20.12s", "123456789abcdefghijk");

    testi2("a=%~ b=%u", -1, -1);
    testi2("a=#%~ b=%x", 17, 255);
    testi2("a=%.*~", 8, 1999);
    testi2("a=%.*~", -8, 1999);
    testi2("a=%+*~",  -7, -1001);

    testdouble("%.4E", -1e-100);
    testdouble("%+8.2E", 1.999e100);
    testdouble("%+14.2E", -1.999e100);
    testdouble("%.2E", 1.0/0.0);
    testdouble("%a", 0.0);
    testdouble("%a", 1.0);
    testdouble("%A", 109);
    testdouble("%A", -0.1);

    testf("%~", 1.0);
    testf("%~", -21.1);
    testf("%~", 10000000000.0);
    testf("%#~", 10000000000.0);
    testf("zero=%~", 0.0);
    testf("minus zero=%~", -0.0);
    testf("%19~", -1.0/0.0);
    testf("%+20~", 1.0/0.0);
    testf("%+1~", 0.0/0.0);
    testf("%.3~", 1234.1);
    testf("%.3~", 12.37);
    testf("%.3~", 9.8765e-4);
    testf("%~", 9.8765e-5);
    testf("%+010.1~", 8.5);
    testf("% 010.1~", 9.5);
    testf("% 05.2~", 755.123);
    testf("%# 05.0~", -19.0);

    //
    // tests for %n
    //
    {
        int v = 0;
        unsigned short vs[4];

        mysnprintf(buf, sizeof(buf), "%%%ny", &v);
        if (strcmp(buf, "%y") != 0) {
            fail++;
            printf("%%n test: got [%s] instead of [%%y]\n", buf);
        }
        if (v != 1) {
            fail++;
            printf("%%n test: v=%d\n", v);
        }
        mysnprintf(buf, sizeof(buf), "d=%d%n", v, &v);
        if (v != 3) {
            fail++;
            printf("%%n test: v=%d instead of 3\n", v);
        }
        memset(vs, 0xab, sizeof(vs));
        mysnprintf( buf, sizeof(buf), "s=%s%hn", "hello", &vs[0] );
        if (vs[0] != 7) {
            fail++;
            printf("short %%n test: got %d\n", vs[0]);
        }
        if (vs[1] != 0xabab) {
            fail++;
            printf("short %%n test: overflow?\n");
        }
    }

    if (fail == 0) printf("ok\n");
    return 0;
}
#endif
