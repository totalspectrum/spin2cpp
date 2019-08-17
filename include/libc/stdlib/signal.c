#include <signal.h>
#include <errno.h>

sighandler_t __sigs[_NSIGS] = { 0 };
extern void _NORETURN _Exit(int);

sighandler_t signal(int sig, sighandler_t handler)
{
  sighandler_t old;

  if (sig < 0 || sig >= _NSIGS)
    {
      errno = EINVAL;
      return SIG_ERR;
    }
  old = __sigs[sig];
  __sigs[sig] = handler;
  return old;
}

int raise(int sig)
{
  sighandler_t func;
  if (sig < 0 || sig >= _NSIGS)
    {
      errno = EINVAL;
      return -1;
    }
  func = __sigs[sig];
  if (func == SIG_IGN)
    return 0;
  if (func == SIG_DFL)
    _Exit(0x20 + sig);
  __sigs[sig] = SIG_DFL;
  (*func)(sig);
  return 0;
}

void
__div_by_zero(void)
{
  raise(SIGFPE);
}
