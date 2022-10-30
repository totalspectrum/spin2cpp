#include <stdarg.h>
#include <compiler.h>
#include <sys/types.h>
#include "sys/fmt.h"

#ifdef __FLEXC__
# ifdef __FEATURE_FLOATS__
#  define INCLUDE_FLOATS
# endif
#else
#define INCLUDE_FLOATS
#endif
#define LONGLONG_SUPPORT

#define va_ptr          va_list *
#define va_ptrarg(x, t) va_arg(*x, t)

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
static const char *parseflags(const char *fmt, unsigned *flag_p)
{
    int c;
    unsigned flags = 0;
    int padchar = PADCHAR_SPACE;
    int signchar = SIGNCHAR_NONE;
    int justify = JUSTIFY_RIGHT;
    int done = 0;
    
    while (!done) {
        c = *fmt++;
        switch (c) {
        case '-':
            justify = JUSTIFY_LEFT;
            break;
        case '#':
            flags |= ALTFMT_BIT;
            break;
        case '+':
            signchar = SIGNCHAR_PLUS;
            break;
        case '0':
            padchar = PADCHAR_ZERO;
            break;
        case ' ':
            padchar = PADCHAR_SPACE;
            break;
        default:
            done = 1;
            break;
        }
    }
    flags |= (padchar << PADCHAR_BIT);
    flags |= (signchar << SIGNCHAR_BIT);
    flags |= (justify << JUSTIFY_BIT);
    *flag_p = flags;
    return fmt-1;
}

// parse the flag indicators ('l', 'j', etc.)
static const char *parsesize(const char *fmt, unsigned *size_p)
{
    int c;
    unsigned size = sizeof(int);
    int longflag = 0;
    c = *fmt++;

    switch (c) {
    case 'l':
        size = sizeof(long);
        longflag = 1;
        if (*fmt == 'l') {
            size = sizeof(long long);
            fmt++;
        }
        break;
    case 'h':
        size = sizeof(short);
        if (*fmt == 'h') {
            size = sizeof(char);
            fmt++;
        }
        break;
    case 'j':
        size = sizeof(uintmax_t);
        break;
    case 'z':
    case 't':  // ASSUMPTION: sizeof(ptrdiff_t) == sizeof(size_t)
        size = sizeof(size_t);
        break;
    case 'L':
        size = sizeof(long double);
        longflag = 1;
        break;
    default:
        // we incremented one too far
        --fmt;
        break;
    }
    *size_p = size;
    return fmt;
}

union f_or_i {
    float f;
    unsigned int i;
};

#ifdef INCLUDE_FLOATS
static inline float _asfloat(unsigned int x)
{
    union f_or_i v;
    v.i = x;
    return v.f;
}
#endif

int _dofmt(putfunc fn, const char *fmtstr, va_list *args)
{
    int c;
    int q;
    int i;
    int bytes_written = 0;
    unsigned flags;
    int width = 0;
    int prec = 0;
    int size;
    int padchar = PADCHAR_NONE;
    unsigned val;
#ifdef LONGLONG_SUPPORT    
    unsigned long long val_LL;
    int is_ll;
#endif    
    for(;;) {
        is_ll = 0;
        c = *fmtstr++;
        if (!c) break;
        if (c != '%') {
            q = PUTC(fn, c);
            if (q < 0) return q;
            bytes_written++;
            continue;
        }
        fmtstr = parseflags(fmtstr, &flags);
        padchar = (flags>>PADCHAR_BIT) & PADCHAR_MASK;
        width = parseint(&fmtstr, args);
        c = *fmtstr; if (c == 0) break;
        if (c == '.') {
            fmtstr++;
            prec = parseint(&fmtstr, args) + 1;
            c = *fmtstr; if (c == 0) break;
        } else {
            prec = 0;
        }
        fmtstr = parsesize(fmtstr, &size);
        c = *fmtstr++; if (c == 0) break;
        // handle some special cases
        if (c == '%') {
            q = _fmtchar(fn, flags, '%');
            continue;
        }
        q = 0;
#ifdef LONGLONG_SUPPORT        
        if (size == 8) {
            val_LL = va_ptrarg(args, unsigned long long);
            val = val_LL;
            is_ll = 1;
        }
        else
#endif            
        {
            val = va_ptrarg(args, unsigned int);
        }
        if (c >= 'A' && c <= 'Z') {
            flags |= (1<<UPCASE_BIT);
            c -= 'A';
            c += 'a';
        }
        if (prec < 0) {
            prec = 0;
        }
        if (prec > PREC_MASK) prec = PREC_MASK;
        if (width < 0) {
            width = -width;
            flags &= ~(JUSTIFY_MASK<<JUSTIFY_BIT);
            flags |= (JUSTIFY_LEFT<<JUSTIFY_BIT);
            padchar = PADCHAR_SPACE;
        }
        flags = flags | (width << MINWIDTH_BIT);
        flags = flags | (prec << PREC_BIT);
        switch (c) {
        case 'c':
            q = _fmtchar(fn, flags, val);
            break;
        case 's':
            if (prec) {
                flags |= ((prec-1) << MAXWIDTH_BIT);
            }
            q = _fmtstr(fn, flags, (const char *)val);
            break;
        case 'd':
        case 'i':
        case 'u':
            if (c == 'u') flags |= (SIGNCHAR_UNSIGNED<<SIGNCHAR_BIT);
            if (prec == 0 && padchar == PADCHAR_ZERO) {
                flags |= ((width+1)<<PREC_BIT);
            }
            if (!is_ll) {
                q = _fmtnum(fn, flags, val, 10);
            } else {
                q = _fmtnumlong(fn, flags, val_LL, 10);
            }
            break;
        case 'o':
            flags |= SIGNCHAR_UNSIGNED << SIGNCHAR_BIT;
            if (prec == 0 && padchar == PADCHAR_ZERO) {
                flags |= ((width+1)<<PREC_BIT);
            }
            if (!is_ll) {
                q = _fmtnum(fn, flags, val, 8);
            } else {
                q = _fmtnumlong(fn, flags, val_LL, 8);
            }
            break;
        case 'x':
            if (prec == 0 && padchar == PADCHAR_ZERO) {
                flags |= ((width+1)<<PREC_BIT);
            }
            flags |= SIGNCHAR_UNSIGNED << SIGNCHAR_BIT;
            if (!is_ll) {
                q = _fmtnum(fn, flags, val, 16);
            } else {
                q = _fmtnumlong(fn, flags, val_LL, 16);
            }
            break;
#ifdef INCLUDE_FLOATS            
        case 'a':
        case 'e':
        case 'f':
        case 'g':
            q = _fmtfloat(fn, flags, _asfloat(val), c);
            break;
#endif            
        default:
            q = _fmtstr(fn, flags, "???");
            break;
        }
        if (q < 0) {
            return q;
        }
        bytes_written += q;
    }
    return bytes_written;
}
