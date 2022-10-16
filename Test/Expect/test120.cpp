#define __SPIN2CPP__
#include <propeller.h>
#include "test120.h"

__asm__(
"    .text\n"
"    .balign 4\n"
"__cog_xfer\n"
"    fcache #(.Lend - .Lstart)\n"
"    .compress off\n"
".Lstart\n"
"    mov pc, lr\n"
"    mov lr, __LMM_RET\n"
"    movd (.Linstr - .Lstart) + __LMM_FCACHE_START,r0\n"
"    movs (.Linstr - .Lstart) + __LMM_FCACHE_START,r1\n"
"    mov  r0,r2\n"
".Linstr\n"
"    mov  0-0,0-0\n"
"    jmp  lr\n"
".Lend\n"
"    .compress default\n"
);
extern "C" int32_t _cog_xfer(int32_t dst, int32_t src, int32_t retval);
#define cogmem_get__(addr)      _cog_xfer(0, (addr), 0)
#define cogmem_put__(addr,data) _cog_xfer((addr), 0, (data))

void test120::Flip0(int32_t I)
{
  cogmem_put__((I | 0x1f0), (cogmem_get__((I | 0x1f0)) ^ 0x1));
}

