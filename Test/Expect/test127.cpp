// check that return value processing goes through
// case statements
#include <propeller.h>
#include "test127.h"

int32_t test127::Asc2val(char *P_str)
{
  int32_t 	C;
  int32_t 	_tmp__0001;
  while (1) {
    C = P_str[0];
    _tmp__0001 = C;
    if (_tmp__0001 == 32) {
      goto _case__0003;
    }
    if ((_tmp__0001 >= '0') && (_tmp__0001 <= '9')) {
      goto _case__0004;
    }
    goto _case__0005;
    _case__0003: ;
    (P_str++);
    goto _endswitch_0002;
    _case__0004: ;
    return (C - '0');
    goto _endswitch_0002;
    _case__0005: ;
    return 0;
    goto _endswitch_0002;
    _endswitch_0002: ;
  }
}

