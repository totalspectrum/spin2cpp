#include <propeller.h>
#include "test152.h"

int32_t test152::Hexdigit(int32_t X)
{
  int32_t 	_tmp__0001;
  _tmp__0001 = X;
  if ((((((_tmp__0001 == 97) || (_tmp__0001 == 98)) || (_tmp__0001 == 99)) || (_tmp__0001 == 100)) || (_tmp__0001 == 101)) || (_tmp__0001 == 102)) {
    goto _case__0003;
  }
  if ((((((_tmp__0001 == 65) || (_tmp__0001 == 66)) || (_tmp__0001 == 67)) || (_tmp__0001 == 68)) || (_tmp__0001 == 69)) || (_tmp__0001 == 70)) {
    goto _case__0004;
  }
  goto _endswitch_0002;
  _case__0003:
  return 1;
  goto _endswitch_0002;
  _case__0004:
  return 1;
  goto _endswitch_0002;
  _endswitch_0002:
  return 0;
}

