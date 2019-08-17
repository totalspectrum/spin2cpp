#ifndef _SYS_FENV_H
#define _SYS_FENV_H

  typedef struct fenv_struct {
    unsigned char exceptions;
    unsigned char roundingmode;
  } _fenv_t;

#endif
