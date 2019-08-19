#ifndef FENV_H_
#define FENV_H_

#define FE_DOWNWARD 1
#define FE_UPWARD   2
#define FE_TOWARDZERO 3
#define FE_TONEAREST 0

int fesetround(int mode);

#define FE_UNDERFLOW 0
#define FE_OVERFLOW  1
#define FE_INEXACT   2
#define FE_INVALID   3
#define FE_DIVBYZERO 4

#endif
