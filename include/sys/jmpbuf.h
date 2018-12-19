#ifndef _SYS_JMPBUF_H
#define _SYS_JMPBUF_H

#ifdef __propeller__
#define _JBLEN 9
#ifdef __GNUC__
/* register offsets in the jmp buffer */
#define _REG_R8  0
#define _REG_R9  1
#define _REG_R10 2
#define _REG_R11 3
#define _REG_R12 4
#define _REG_R13 5
#define _REG_R14 6
#define _REG_LR  7
#define _REG_SP  8
#endif
#else
#error "unknown machine type"
#endif

#ifndef _JBTYPE
#define _JBTYPE unsigned long
#endif

typedef _JBTYPE _jmp_buf[_JBLEN];

#endif
