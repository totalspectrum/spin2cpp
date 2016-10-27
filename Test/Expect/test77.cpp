#include <propeller.h>
#include "test77.h"

static uint32_t Sqrt__(uint32_t a) {
    uint32_t res = 0;
    uint32_t bit = 1<<30;
    while (bit > a) bit = bit>>2;
    while (bit != 0) {
        if (a >= res+bit) {
            a = a - (res+bit);
            res = (res>>1) + bit;
        } else res = res >> 1;
        bit = bit >> 2;
    }
    return res;
}
char test77::dat[] = {
  0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40, 0x40, 0xf3, 0x04, 0xb5, 0x3f, 
};
int32_t *test77::Dummy(void)
{
  return (((int32_t *)&dat[0]));
}

