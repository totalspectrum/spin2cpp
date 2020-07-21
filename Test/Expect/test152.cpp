#include <propeller.h>
#include "test152.h"

int32_t test152::Hexdigit(int32_t X)
{
  int32_t 	_tmp__0000;
  _tmp__0000 = X;
  if ((((((_tmp__0000 == 97) || (_tmp__0000 == 98)) || (_tmp__0000 == 99)) || (_tmp__0000 == 100)) || (_tmp__0000 == 101)) || (_tmp__0000 == 102)) {
    goto _case__0002;
  }
  if ((((((_tmp__0000 == 65) || (_tmp__0000 == 66)) || (_tmp__0000 == 67)) || (_tmp__0000 == 68)) || (_tmp__0000 == 69)) || (_tmp__0000 == 70)) {
    goto _case__0003;
  }
  goto _endswitch_0001;
  _case__0002: ;
  return 1;
  goto _endswitch_0001;
  _case__0003: ;
  return 1;
  goto _endswitch_0001;
  _endswitch_0001: ;
  return 0;
}

