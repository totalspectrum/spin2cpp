#include <propeller.h>
#include "test123.h"

typedef void (*Cogfunc__)(void *a, void *b, void *c, void *d);
static void Cogstub__(void **arg) {
  Cogfunc__ func = (Cogfunc__)(arg[0]);
  func(arg[1], arg[2], arg[3], arg[4]);
}
extern "C" void _clone_cog(void *tmp);
static int32_t Coginit__(int cogid, void *stacktop, void *func, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4) {
    void *tmp = __builtin_alloca(1984);
    unsigned int *sp = (unsigned int *)stacktop;
    static int32_t cogargs__[5];
    int r;
    cogargs__[0] = (int32_t) func;
    cogargs__[1] = arg1;
    cogargs__[2] = arg2;
    cogargs__[3] = arg3;
    cogargs__[4] = arg4;
    _clone_cog(tmp);
    *--sp = 0;
    *--sp = (unsigned int)cogargs__;
    *--sp = (unsigned int)Cogstub__;
    r = coginit(cogid, tmp, sp);
    return r;
}
void test123::Demo(void)
{
  Coginit__(-1, (void *)&Stack[64], (void *)Hello, 0, 0, 0, 0);
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
  waitcnt((40000000 + CNT));
}

