/*
 * from the libnix library for the Amiga, written by
 * Matthias Fleischer and Gunther Nikl and placed in the public
 * domain
 * wide character changes by Eric Smith
 */
#include <stdio.h>
#include <wctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

unsigned long wcstoul(const wchar_t *nptr,wchar_t **endptr,int base)
{ const wchar_t *p=nptr,*q;
  wchar_t c=0;
  unsigned long r=0;
  if(base<0||base==1||base>36)
  { if(endptr!=NULL)
      *endptr=(wchar_t *)nptr;
    return 0;
  }
  while(iswspace(*p))
    p++;
  if(*p==L'-'||*p==L'+')
    c=*p++;
  if(base==0)
  { if(p[0]==L'0')
    { if(towlower(p[1])==L'x'&&iswxdigit(p[2]))
      { p+=2;
        base=16; }
      else
        base=8;
    }else
      base=10;
  }
  q=p;
  for(;;)
  { int a;
    int dig;
    dig=*q;
    if(!iswalnum(dig))
      break;
    a=(dig>='0'&&dig<='9')?dig-L'0':towlower(dig)-(L'a'-10);
    if(a>=base)
      break;
    if(r>(ULONG_MAX-a)/base||r*base>ULONG_MAX-a)
    { errno=ERANGE; /* overflow */
      r=ULONG_MAX; }
    else
      r=r*base+a;
    q++;
  }
  if(q==p) /* Not a single number read */
  { if(endptr!=NULL)
      *endptr=(wchar_t *)nptr;
    return 0;
  }
  if(c=='-')
    r=-r;
  if(endptr!=NULL)
    *endptr=(wchar_t *)q;
  return r;
}
