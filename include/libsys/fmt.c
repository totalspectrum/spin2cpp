#include "fmt.h"

//
// reverse a string in-place
//
void _strrev(char *str)
{
    char *end;
    int c;

    if (!*str) return;
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
    int minwidth = (fmt >> MINWIDTH_BIT) & 0xff;
    unsigned justify = (fmt >> JUSTIFY_BIT) & 3;
    int i, r;
    int n = 0;
    if (justify == 0) {
        justify = JUSTIFY_LEFT;
    }
    if ( (justify & leftright) == 0 ) {
        return 0;  // padding goes on other side
    }
    width = minwidth - width; // (amount to pad)
    if (width <= 0) {
        return 0;
    }
    if (justify == JUSTIFY_CENTER) { // centering, only half as much padding (if odd do on right)
        width = (width + (leftright==PAD_ON_RIGHT)) / 2;
    }
    for (i = 0; i < width; i++) {
        r = PUTC(fn, ' ');
        if (r < 0) return r;
        n += r;
    }
    return n;
}

int _fmtstr(putfunc fn, unsigned fmt, const char *str)
{
    int maxwidth = (fmt >> MAXWIDTH_BIT) & 0xff;
    int width = strlen(str);
    int n;
    int r, i;

    if (maxwidth && width > maxwidth) {
        width = maxwidth;
    }
    n = _fmtpad(fn, fmt, width, PAD_ON_LEFT);
    if (n < 0) return n;
    for (i = 0; i < width; i++) {
        r = PUTC(fn, *str++);
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
    c &= 255;
    return _fmtstr(fn, fmt, (char*)&c);
}

#ifdef SMALL_INT
int _uitoall(char *orig_str, unsigned long long num, unsigned base, unsigned mindigits, int uppercase)
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
#ifdef _DEBUG_PRINTF
        __builtin_printf("uitoall: num=%x::%x digit=%c\n", (unsigned)(num>>32), (unsigned)(num), digit);
#endif        
        *str++ = digit;
        num = num / base;
        width++;
    } while (num > 0 || width < mindigits);
    *str++ = 0;
    _strrev(orig_str);
    return width;
}
#endif

int _uitoa(char *orig_str, UITYPE num, unsigned base, unsigned mindigits, int uppercase)
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
    } while (num != 0 || width < mindigits);
    *str++ = 0;
    _strrev(orig_str);
    return width;
}

#define MAX_NUM_DIGITS 64

int _fmtnum(putfunc fn, unsigned fmt, int x, int base)
{
    #ifdef __OUTPUT_ASM__
    char *buf = __builtin_alloca(MAX_NUM_DIGITS+1);
    #else
    char buf[MAX_NUM_DIGITS+1];
    #endif
    char *ptr = buf;
    int width = 0;
    int mindigits = (fmt >> PREC_BIT) & PREC_MASK;
    int maxdigits = (fmt >> MAXWIDTH_BIT) & 0xff;
    int signchar = (fmt >> SIGNCHAR_BIT) & 0x3;

    if (mindigits > 0) {
        // prec gets offset by one to allow 0 to be "default"
        mindigits = mindigits - 1;
    }
    if (maxdigits > MAX_NUM_DIGITS || maxdigits == 0) {
        maxdigits = MAX_NUM_DIGITS;
    }
    if (signchar == SIGNCHAR_UNSIGNED) {
        signchar = SIGNCHAR_NONE;
    } else if (x < 0) {
        signchar = SIGNCHAR_MINUS;
        x = -x;
    }
    if (signchar != SIGNCHAR_NONE) {
        width++;
        if (mindigits == maxdigits) {
            mindigits--;
            if (!mindigits) {
                return _fmtchar(fn, fmt, '#');
            }
        }
        if (signchar == SIGNCHAR_SPACE) {
            *ptr++ = ' ';
        } else if (signchar == SIGNCHAR_MINUS) {
            *ptr++ = '-';
        } else {
            *ptr++ = '+';
        }
    }
    width += _uitoa(ptr, x, base, mindigits, 0 != (fmt & (1<<UPCASE_BIT)));
    if (width > maxdigits) {
        while (maxdigits-- > 0) {
            *ptr++ = '#';
        }
        *ptr++ = 0;
    }
    return _fmtstr(fn, fmt, buf);
}

int _fmtnumlong(putfunc fn, unsigned fmt, long long x, int base)
{
    #ifdef __OUTPUT_ASM__
    char *buf = __builtin_alloca(MAX_NUM_DIGITS+1);
    #else
    char buf[MAX_NUM_DIGITS+1];
    #endif
    char *ptr = buf;
    int width = 0;
    int mindigits = (fmt >> PREC_BIT) & PREC_MASK;
    int maxdigits = (fmt >> MAXWIDTH_BIT) & 0xff;
    int signchar = (fmt >> SIGNCHAR_BIT) & 0x3;

    if (mindigits > 0) {
        // prec gets offset by one to allow 0 to be "default"
        mindigits = mindigits - 1;
    }
    if (maxdigits > MAX_NUM_DIGITS || maxdigits == 0) {
        maxdigits = MAX_NUM_DIGITS;
    }
    if (signchar == SIGNCHAR_UNSIGNED) {
        signchar = SIGNCHAR_NONE;
    } else if (x < 0) {
        signchar = SIGNCHAR_MINUS;
        x = -x;
    }
    if (signchar != SIGNCHAR_NONE) {
        width++;
        if (mindigits == maxdigits) {
            mindigits--;
            if (!mindigits) {
                return _fmtchar(fn, fmt, '#');
            }
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
    width += _uitoall(ptr, x, base, mindigits, 0 != (fmt & (1<<UPCASE_BIT)));
    if (width > maxdigits) {
        while (maxdigits-- > 0) {
            *ptr++ = '#';
        }
        *ptr++ = 0;
    }
    return _fmtstr(fn, fmt, buf);
}

#ifdef INCLUDE_FLOATS
/*******************************************************
 * floating point support
 *******************************************************/
typedef union DI {
    FTYPE d;
    UITYPE i;
} DI;

#ifdef DEBUG_PRINTF
UITYPE asInt(FTYPE d)
{
    DI u;
    u.d = d;
    return u.i;
}
#endif


#ifdef __propeller__

# ifdef __GNUC__
extern long double _intpow(long double a, long double b, int n);
#define _float_pow_n(b, a, n) _intpow(b, a, n)
# endif
# ifdef __FLEXC__
// nothing to do here
# endif
#else
// calculate a * b^n with as much precision as possible
double _float_pow_n(long double a, long double b, int n)
{
    long double r = powl((long double)b, (long double)n);
    return a*r;
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
#ifdef __fixedreal__
#define DOUBLE_BITS 16
#define MAX_DEC_DIGITS 7
#define DOUBLE_ONE ((unsigned)(1<<DOUBLE_BITS))
#define DOUBLE_MASK ((DOUBLE_ONE) - 1)

static void disassemble(FTYPE x, UITYPE *aip, int *np, int numdigits, int base)
{
    DI un;
    UITYPE ai;
    UITYPE afrac;
    UITYPE limit;
    FTYPE based;
    int n;
    int maxdigits;
    
    if (x == 0.0) {
        *aip = 0;
        *np = 0;
        return;
    }
    un.d = x;
    ai = un.i;

    // base 2 we will group digits into 4 to print as hex
    if (base == 2) {
        numdigits *= 4;
        maxdigits = 24;
    } else {
        maxdigits = MAX_DEC_DIGITS;
    }

    afrac = ai & DOUBLE_MASK;
    ai = ai >> DOUBLE_BITS;

    n = 0;
    based = (float)base;
    while (x >= based) {
        n++;
        x = x / based;
    }
    while (x < 1.0) {
        --n;
        x *= based;
    }
    if (numdigits < 0) {
        numdigits = n - (numdigits+1);
        if (numdigits < 0) {
            *aip = 0;
            *np = 0;
            return;
        }
    } else {
        numdigits = numdigits+1;
    }
    if (numdigits > maxdigits) {
        numdigits = maxdigits;
    }

    limit = base;
    n = numdigits;
    --numdigits;
    while (numdigits > 0) {
        limit = limit * base;
        --numdigits;
    }

    while (ai < limit) {
#ifdef _DEBUG_PRINTF
        __builtin_printf("ai=%d n=%d\n", ai, n);
#endif        
        --n;
        afrac = afrac * base;
        ai = (ai * base) + (afrac >> DOUBLE_BITS);
        afrac = afrac & DOUBLE_MASK;
    }
    limit = limit*base;
    while (ai >= limit) {
        UITYPE d;
        d = ai % base;
        ai = ai / base;
        afrac = (afrac + (d<<DOUBLE_BITS)) / base;
        ++n;
    }
    if (afrac >= (1<<(DOUBLE_BITS-1))) {
        ai++;
        if (ai >= limit) {
            ++n;
            ai = ai / base;
        }
    }
    *aip = ai;
    *np = n;
}

#else
#ifdef SMALL_INT
#define DOUBLE_BITS 23
#define MAX_DEC_DIGITS 9
#define DOUBLE_ONE ((unsigned)(1<<DOUBLE_BITS))
#else
#define DOUBLE_BITS 52
#define MAX_DEC_DIGITS 17
#define DOUBLE_ONE (1ULL<<DOUBLE_BITS)
#endif

#define DOUBLE_MASK (DOUBLE_ONE-1)

static void disassemble(FTYPE x, UITYPE *aip, int *np, int numdigits, int base)
{
    FTYPE a;
    int maxdigits;
    FTYPE based = (FTYPE)base;
    UITYPE ai;
    UITYPE u;
    UITYPE maxu;
    int n;
    int i;
    int trys = 0;
    DI un;

    if (x == 0.0) {
        *aip = 0;
        *np = 0;
        return;
    }

    // first, find (a,n) such that
    // 1.0 <= a < base and x = a * base^n
    n = ilogb(x);
    if (base == 10) {
        // initial estimate: 2^10 ~= 10^3, so 3/10 of ilogb is a good first guess
        n = (3 * n)/10 ;
        maxdigits = MAX_DEC_DIGITS;
    } else {
        maxdigits = DOUBLE_BITS+1;  // for base 2
    }
    
    while (trys++ < 8) {
        FTYPE p;
        p = _float_pow_n(1.0, based, n);
        a = x / p;
        if (a < 1.0) {
            --n;
        } else if (a >= based) {
            ++n;
        } else {
            break;
        }
    }
#if defined(_TEST_FMT) && !defined(__FLEXC__)
    if (trys == 8) {
        fprintf(stderr, "Warning hit retry count\n");
    }
#endif
    i = ilogb(a);
    un.d = a;
    ai = un.i & DOUBLE_MASK;
    ai |= DOUBLE_ONE;
    ai = ai<<i;
    // base 2 we will group digits into 4 to print as hex
    if (base == 2) {
        numdigits *= 4;
    }
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
        UITYPE d;
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
    if (ai > (unsigned)(base*DOUBLE_ONE/2) || (ai == (unsigned)(base*DOUBLE_ONE/2) && (u & 1))) {
        u++;
        if (u == maxu) {
            ++n;
        }
    }
done:
    *aip = u;
    *np = n;
}
#endif

//
// output the sign and any hex digits required
// if buf is NULL print using pi
// otherwise put into buf
// returns number of bytes output, or -1 if an error happens
//
static int
emitsign(char *buf, int sign, int hex)
{
    int r = 0;
    if (sign) {
        *buf++ = sign;
        r++;
    }
    if (hex) {
        *buf++ = '0';
        *buf++ = hex;
        r += 2;
    }
    return r;
}

//
// find the digits for x 
// we will output "prec" digits after the decimal point
// spec is a C printf style specification ('g', 'f', 'a', etc.)
//

#define MAXWIDTH 64

int _fmtfloat(putfunc fn, unsigned fmt, FTYPE x, int spec)
{
    UITYPE ai;
    int i;
    int base = 10;
    int exp;  // exponent
    int isExpFmt = 0;  // output should be printed in exponential notation
    int totalWidth;
    int sign = 0;
    int expchar;
    int stripTrailingZeros = 0;
    int expprec = 2;
    int needPrefix = 0;
    int hexSign = 0;
    int padkind = 0;
    int no_oflow = 0;
    
    // we print a series of 0's, then the digits, then more 0's if necessary
    // to force e.g. 4 leading 0's, set startdigit to -4
    int startdigit;
    int decpt;
    int enddigit;
    int numdigits;
    int expdigits;
    int expsign = 0;
    int signclass;
    int minwidth;
    int maxwidth;
    char dig[64]; // digits for the number
    char expdig[8]; // digits for the exponent
    int prec;
    int needUpper;
    char *buf;
    char *origbuf;
    char tmpbuf[MAXWIDTH + 1];
    int justify;
    int hash_format;
    
    origbuf = buf = tmpbuf;

    prec = (fmt >> PREC_BIT) & PREC_MASK;
    hash_format = (fmt >> ALTFMT_BIT) & 1;
    
    if (prec == 0) {
        // if precision is missing, %a needs enough digits
        // to precisely represent the number
        if (spec == 'a') {
            prec = 13;
            stripTrailingZeros = 1;
        } else {
            prec = DEFAULT_PREC;
        }
    } else {
        prec = prec-1;
    }
    justify = (fmt >> JUSTIFY_BIT) & JUSTIFY_MASK;
    needUpper = (fmt >> UPCASE_BIT) & 1;
    minwidth = (fmt >> MINWIDTH_BIT) & WIDTH_MASK;
    maxwidth = (fmt >> MAXWIDTH_BIT) & WIDTH_MASK;
    isExpFmt = (spec == 'e');
    expchar = needUpper ? 'E' : 'e';
    if (spec == 'a') {
        isExpFmt = 1;
        expchar = needUpper ? 'P' : 'p';
        base = 2;
        expprec = 1;
        hexSign = needUpper ? 'X' : 'x';
    }

    if (spec == '#') {
        no_oflow = 1;
        if (hash_format) {
            spec = 'f';
            hash_format = 0;
        } else {
            hash_format = 1;
        }
    }
    signclass = (fmt >> SIGNCHAR_BIT) & SIGNCHAR_MASK;
    
    if ( signbit(x) ) {
        // work on non-negative values only
        sign = '-';
        x = -x;
    } else {
        // sometimes we need positive sign
        if (signclass == SIGNCHAR_PLUS) {
            sign = '+';
        } else if (signclass == SIGNCHAR_SPACE) {
            sign = ' ';
        }
    }

    // if user requested 0 padding, emit the sign and adjust width
    // now
    padkind = (fmt >> PADCHAR_BIT) & PADCHAR_MASK;
    needPrefix = (sign || base != 10);
    if ( needPrefix && padkind == PADCHAR_ZERO && (justify != JUSTIFY_RIGHT)) {
        int r;
        r = emitsign(buf, sign, hexSign);
        if (r < 0) return r;
        buf += r;
        if (minwidth) {
            minwidth -= r;
            if (minwidth < 0) minwidth = 0;
            fmt = fmt & ~(WIDTH_MASK << MINWIDTH_BIT);
            fmt |= (minwidth << MINWIDTH_BIT);
        }
        needPrefix = 0;
    }

    // handle special cases
    if (isinf(x)) {
        // emit sign
        if (sign) *buf++ = sign;
        strcpy(buf, "inf");
        goto done;
    } else if (isnan(x)) {
        if (sign) *buf++ = sign;
        strcpy(buf, "nan");
        goto done;
    }

    if (spec == 'g' || spec == '#') {
        // find the exponent
        disassemble(x, &ai, &exp, prec, base);
        if (spec == '#') {
            if (exp > prec) {
                isExpFmt = 1;
            } else if (exp < 0) {
                if (exp <= -prec) {
                    isExpFmt = 1;
                }
            } else if (exp > 0) {
                prec = prec - exp;
            }
        } else {
            // for g format, special handling
            stripTrailingZeros = (0 == ((fmt>>ALTFMT_BIT) & 1));
            if (exp >= prec || exp < -4) {
                isExpFmt = 1;
            } else {
                prec = prec - exp;
                disassemble(x, &ai, &exp, -prec, base);
            }
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
#ifdef SMALL_INT
        // we actually want hex output, but with small
        // integers there are 23 bits after the decimal point
        // rather than 24, so adjust here
        while (ai && ai < (DOUBLE_ONE << 1)) {
            ai = ai << 1;
        }
#endif        
    }

    numdigits = _uitoa(dig, ai, base, 1, needUpper);
    if (exp < 0) {
        expsign = '-';
        expdigits = _uitoa(expdig, -exp, 10, expprec, needUpper);
    } else {
        expsign = '+';
        expdigits = _uitoa(expdig, exp, 10, expprec, needUpper);
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
    if (totalWidth > MAXWIDTH) {
        return -1;
    }
    // emit sign
    if (needPrefix) {
        int r = emitsign(buf, sign, hexSign);
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
        if (i == decpt && (hash_format || i < (enddigit-1))) {
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
    // now output the string
    if (maxwidth && no_oflow && strlen(origbuf) > maxwidth) {
        int j;
        char *ptr = origbuf;
        //__builtin_printf("maxwidth=%d, minwidth=%d, prec=%d\n", maxwidth, minwidth, prec);
        if (isExpFmt) {
            prec = 0;
            j = maxwidth;
        } else {
            j = (maxwidth-1)-prec;
        }
        while (j) {
            *ptr++ = '*';
            --j;
        }
        if (prec) {
            *ptr++ = '.';
            for (j = 0; j<prec; j++) {
                *ptr++ = '*';
            }
        }
        *ptr++ = 0;
    }
    return _fmtstr(fn, fmt, origbuf);
}
#endif /* INCLUDE_FLOATS */

/*
 * I/O locks
 */
#ifdef _SIMPLE_IO

# if defined(__FEATURE_MULTICOG__) && !defined(_NO_LOCKIO)
static int __iolock;
int __lockio(int h)   { _lockmem(&__iolock); return 0; }
int __unlockio(int h) { _unlockmem(&__iolock); return 0; }
# else
int __lockio(int h)   { return 0; }
int __unlockio(int h) { return 0; }
# endif

#else

static int *_getiolock(unsigned h) {
    vfs_file_t *v;
    static int dummy;
    v = __getftab(h);
    if (!v || !v->state) return &dummy;
    return &v->lock;
}
int __lockio(unsigned h) {
#ifndef _NO_LOCKIO    
    _lockmem(_getiolock(h));
#endif    
    return 0;
}
int __unlockio(unsigned h) {
#ifndef _NO_LOCKIO
    _unlockmem(_getiolock(h));
#endif
    return 0;
}

#endif

#ifdef _TEST_FMT
#include <stdio.h>

int _tx(int c)
{
    putchar(c);
    return 1;
}
int _rx(void)
{
    return -1;
}

void testfloat(float x)
{
#ifdef __FLEXC__
    printf("[");
#else    
    printf("%g -> [", x);
#endif    
    _basic_print_float(0, x, 0);
    printf("]\n");
}

int main()
{
    printf("[");
    _basic_print_integer(0, -99, 0x4830404, 10);
    printf("]\n");
    printf("[");
    _basic_print_integer(0, 98, 0, 10);
    printf("]\n");

    testfloat(0.0);
    testfloat(0.1);
    testfloat(0.01);
    testfloat(100.0);
    testfloat(-99.12345);
    testfloat(1e6);
    
    return 0;
}
#endif
