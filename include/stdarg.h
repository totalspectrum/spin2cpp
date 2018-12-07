#ifndef _STDARG_H_
#define _STDARG_H_

#define va_list unsigned long
#define va_start __builtin_va_start
#define va_arg(vl, typ) __builtin_va_arg(vl, typ)
#define va_end(args)

#endif
