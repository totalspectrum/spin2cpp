''
'' verify errors inside include files
''

PUB foo | x
#include "notfound.h"
  x := 2
  return x
