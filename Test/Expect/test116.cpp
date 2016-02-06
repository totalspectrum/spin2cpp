//
// test for wordfill
//
#include <propeller.h>
#include "test116.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

void test116::Fillone(int32_t X, int32_t Count)
{
  { int32_t _fill__0000; uint16_t *_ptr__0002 = (uint16_t *)X; uint16_t _val__0001 = 1; for (_fill__0000 = Count; _fill__0000 > 0; --_fill__0000) {  *_ptr__0002++ = _val__0001; } };
}

