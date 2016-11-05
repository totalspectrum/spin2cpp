#include <propeller.h>
#include "test123.h"

typedef void (*Cogfunc__)(void *a, void *b, void *c, void *d);
static void Cogstub__(void *argp) {
  void **arg = (void **)argp;
  Cogfunc__ func = (Cogfunc__)(arg[0]);
  func(arg[1], arg[2], arg[3], arg[4]);
}
__asm__(".global _cogstart\n"); // force clone_cog to link if it is present
extern "C" void _clone_cog(void *tmp) __attribute__((weak));
extern "C" long _load_start_kernel[] __attribute__((weak));
static int32_t Coginit__(int cogid, void *stackbase, size_t stacksize, void *func, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4) {
    void *tmp = _load_start_kernel;
    unsigned int *sp = ((unsigned int *)stackbase) + stacksize/4;
    static int32_t cogargs__[5];
    int r;
    cogargs__[0] = (int32_t) func;
    cogargs__[1] = arg1;
    cogargs__[2] = arg2;
    cogargs__[3] = arg3;
    cogargs__[4] = arg4;
    if (_clone_cog) {
        tmp = __builtin_alloca(1984);
        _clone_cog(tmp);
    }
    *--sp = 0;
    *--sp = (unsigned int)cogargs__;
    *--sp = (unsigned int)Cogstub__;
    r = coginit(cogid, tmp, sp);
    return r;
}
void test123::Demo(void)
{
  Coginit__(30, (void *)Stack, 256, (void *)Hello, 0, 0, 0, 0);
  Pauseabit();
  Pauseabit();
  Exit();
}

void test123::Hello(test123 *self)
{
  self->Fds.Str((int32_t)"Hello, World\r\n");
}

void test123::Exit(void)
{
  Fds.Txflush();
  Fds.Stop();
}

void test123::Pauseabit(void)
{
  waitcnt(40000000 + CNT);
}

