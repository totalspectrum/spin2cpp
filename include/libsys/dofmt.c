#include <stdarg.h>
#include <compiler.h>
#include "sys/fmt.h"

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
    unsigned val;
#ifdef LONGLONG_SUPPORT    
    unsigned long long val_LL;
#endif    
    for(;;) {
        c = *fmtstr++;
        if (!c) break;
        if (c != '%') {
            q = (*fn)(c);
            if (q < 0) return q;
            bytes_written++;
            continue;
        }
        fmtstr = parseflags(fmtstr, &flags);
        width = parseint(&fmtstr, args);
        if (width < 0) {
            width = -width;
            flags &= ~(JUSTIFY_MASK<<JUSTIFY_BIT);
            flags |= (JUSTIFY_LEFT<<JUSTIFY_BIT);
        }
        c = *fmtstr; if (c == 0) break;
        if (c == '.') {
            fmtstr++;
            prec = parseint(&fmtstr, args);
            c = *fmtstr; if (c == 0) break;
        }
        fmtstr = parsesize(fmtstr, &size);
        c = *fmtstr++; if (c == 0) break;
        q = 0;
#ifdef LONGLONG_SUPPORT        
        if (size == 8) {
            val_LL = va_ptrarg(args, unsigned long long);
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
        if (prec < 0) prec = 0;
        if (prec > PREC_MASK) prec = PREC_MASK;
        flags = flags | (width << MINWIDTH_BIT);
        flags = flags | (prec << PREC_BIT);
        switch (c) {
        case 'c':
            q = _fmtchar(fn, flags, val);
            break;
        case 's':
            flags = flags | (prec << MAXWIDTH_BIT);
            q = _fmtstr(fn, flags, (const char *)val);
            break;
        case 'd':
        case 'i':
            q = _fmtnum(fn, flags, val, 10);
            break;
        case 'u':
            flags |= SIGNCHAR_UNSIGNED << SIGNCHAR_BIT;
            q = _fmtnum(fn, flags, val, 10);
            break;
        case 'o':
            flags |= SIGNCHAR_UNSIGNED << SIGNCHAR_BIT;
            q = _fmtnum(fn, flags, val, 8);
            break;
        case 'x':
            flags |= SIGNCHAR_UNSIGNED << SIGNCHAR_BIT;
            q = _fmtnum(fn, flags, val, 16);
            break;
        default:
            q = 0;
            break;
        }
        if (q < 0) {
            return q;
        }
        bytes_written += q;
    }
    return bytes_written;
}
