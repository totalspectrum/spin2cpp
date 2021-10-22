// check that return value processing goes through
// case statements
#define __SPIN2CPP__
#include <propeller.h>
#include "test127.h"

int32_t test127::Asc2val(char *P_str)
{
  int32_t 	C, _tmp__0000;
  while (1) {
    C = P_str[0];
    _tmp__0000 = C;
    if (_tmp__0000 == 32) {
      goto _case__0002;
    }
    if ((_tmp__0000 >= '0') && (_tmp__0000 <= '9')) {
      goto _case__0003;
    }
    goto _case__0004;
    _case__0002: ;
    (P_str++);
    goto _endswitch_0001;
    _case__0003: ;
    return (C - '0');
    goto _endswitch_0001;
    _case__0004: ;
    return 0;
    goto _endswitch_0001;
    _endswitch_0001: ;
  }
}

