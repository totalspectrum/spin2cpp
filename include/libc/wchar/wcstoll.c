/*
 * from the libnix library for the Amiga, written by
 * Matthias Fleischer and Gunther Nikl and placed in the public
 * domain
 */
#include <stdio.h>
#include <wctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#include <compiler.h>

signed long long wcstoll(const wchar_t *nptr,wchar_t **endptr,int base)
{ const wchar_t *p=nptr;
  wchar_t *q;
  unsigned long long r;
  while(iswspace(*p))
    p++;
  r=wcstoul(p,&q,base);
  if(endptr!=NULL)
  { if(q==p)
      *endptr=(wchar_t *)nptr;
    else
      *endptr=q;
  }
  if(*p=='-')
  { if((signed long)r>0)
    { errno=ERANGE;
      return LLONG_MIN; }
    else
      return r;
  }else
  { if((signed long)r<0)
    { errno=ERANGE;
      return LLONG_MAX; }
    else
      return r;
  }
}

__weak_alias(wcstoimax, wcstoll);
