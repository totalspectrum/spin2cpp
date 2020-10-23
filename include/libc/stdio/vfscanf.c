/*
 * originally from the libnix library for the Amiga, written by
 * Matthias Fleischer and Gunther Nikl and placed in the public domain
 *
 * Propeller specific changes by Eric R. Smith,
 * Copyright (c)2011 Parallax Inc.
 * Copyright (c)2020 Total Spectrum Software Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <compiler.h>
#include <wchar.h>
#include <string.h>

#ifdef __FLEXC__
#else
#define LONG_LONG_SUPPORT
#endif

#ifdef LONG_LONG_SUPPORT
typedef unsigned long long ULongLong;
typedef signed long long   SLongLong;
#else
typedef unsigned long ULongLong;
typedef signed long   SLongLong;
#endif

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

#define WCHAR_SUBTYPE 'l'

/*
 * define FLOAT_SUPPORT to get floating point support
 */
/*#define FLOAT_SUPPORT*/

/* some macros to cut this short
 * NEXT(c);     read next character
 * PREV(c);     ungetc a character
 * VAL(a)       leads to 1 if a is true and valid
 */
#define NEXT(c) ((c)=mygetc(stream, ms_ptr))
//#define NEXT(c) ((c)=fgetc(stream),ms.size++,ms.incount++)
#define PREV(c) myungetc((c),stream, ms_ptr)
//#define PREV(c) do{if((c)!=EOF)ungetc((c),stream);ms.size--;ms.incount--;}while(0)
#define VAL(a)  ((a)&&ms_ptr->size<=width)

typedef struct mystate {
  size_t size;
  size_t incount;
  mbstate_t mbs;
  int ungot;
} MyState;

/* get a single byte */
static int
mygetc(FILE *stream, MyState *ms)
{
  int mbc;

  if (ms->ungot) {
      mbc = (ms->ungot)-1;
      ms->ungot = 0;
  } else {
      mbc = fgetc(stream);
  }
  ms->size++;
  ms->incount++;
  return mbc;
}

static int in_scanset(int c, const unsigned char *scanset);

/* get a single character from stream; this may be
 * multiple bytes depending on locale
 * c is the first byte of the character
 * returns the last byte read, or EOF
 * "scanset" is a set we should check; if any byte read is not in the
 * scanset abort
 */
static int
mywgetc(int c, wchar_t *wc_ptr, FILE *stream, MyState *ms, const unsigned char *scanset, int *scan_check)
{
  size_t count;

  ms->mbs.left = 0;
  *scan_check = 1;
  do {
    if (c < 0 || !in_scanset(c, scanset)) {
      *scan_check = 0;
      return c;
    }
    /* NOTE: assumes c is little-endian here */
    count = mbrtowc(wc_ptr, (char *)&c, 1, &ms->mbs);
    if (count != ((size_t)-2)) break;
    /* need more bytes */
    if (ms->ungot) {
        c = ms->ungot-1;
        ms->ungot = 0;
    } else {
        c = fgetc(stream);
    }
    ms->incount++;
  } while (1);
  if (count == (size_t)-1) {
    /* this is harsh, but makes us pass the compatiblity test */
    *wc_ptr = 0;
    return EOF;
  }
  return c;
}

static void
myungetc(int c, FILE *stream, MyState *ms)
{
  ms->size--;
  ms->incount--;
  // ideally we'd do this in the stream, but for now punt
  //ungetc(c, stream);
  ms->ungot = c + 1;  
}

/*
 * see if a character is acceptable based on a scan set
 */
static int
in_scanset(int c, const unsigned char *scanset)
{
  int wc, last;
  int inset = 1;

  if (!scanset)
    return inset;
  if (*scanset == '^') {
    inset = 0;
    scanset++;
  }
  if (*scanset == ']') {
    if (c == ']') return inset;
    last = *scanset++;
  }
  for(;;) {
    wc = *scanset++;
    if (wc <= 0 || wc == ']')
      return !inset;
    if (wc == '-' && last && *scanset != ']') {
      wc = *scanset++;
      if (c >= last && c <= wc)
	return inset;
    } else if (c == wc) {
      return inset;
    }
    last = wc;
  }
  return !inset;
}

/*
 * get a string from the stream
 * this may be a wide or narrow string, depending on
 * "is_wchar"
 * dest is where to put it, and width is the maximum number of characters to put
 * c is the next character from the stream
 * ignore is true if we should not assign
 * scanset is NULL if all characters are accepted, otherwise a set of characters
 * to accept or reject
 * "need_zero" is true if we should append a trailing 0
 */
static void
get_string(int is_wchar, void *dest, size_t width, int c, FILE *stream,
	   int ignore, MyState *ms_ptr,
	   const unsigned char *scanset, int need_zero)
{
  unsigned char *bp = NULL;
  wchar_t *wp = NULL;
  wchar_t wc;

  if (is_wchar)
    wp = dest;
  else
    bp = dest;

  while (VAL(c!=EOF))
    {
      if (!ignore)
	{
	  if (is_wchar)
	    {
	      int scan_check;
	      c = mywgetc(c, &wc, stream, ms_ptr, scanset, &scan_check);
	      if (!scan_check)
		break;
	      *wp++ = wc;
	    }
	  else
	    {
	      if (!(in_scanset(c, scanset)))
		break;
	      *bp++ = c;
	    }
	}
      NEXT(c);
    }
  if (need_zero && !ignore && ms_ptr->size) {
    if (is_wchar)
      *wp++ = L'\0';
    else
      *bp++ = 0;
  }
  PREV(c);
}

int vfscanf(FILE *stream,const char *format_ptr,va_list args)
{
  size_t blocks=0;
  int c=0;
  int fmt=0;
  MyState ms, *ms_ptr;

  memset((ms_ptr = &ms), 0, sizeof(ms));

  fmt = *format_ptr++;
  while(fmt)
    {
      ms.size=0;

      if(fmt=='%')
	{
	  size_t width=ULONG_MAX;
	  char type,subtype='i',ignore=0;
	  const unsigned char *ptr=(const unsigned char *)(format_ptr);

	  if(isdigit(*ptr))
	    {
	      width=0;
	      while(isdigit(*ptr))
		width=width*10+(*ptr++-'0');
	    }

	  if (*ptr=='*')
	    {
	      ptr++;
	      ignore=1;
	    }

	  if(*ptr=='h'||*ptr=='l'||*ptr=='L'||*ptr=='j'||*ptr=='t'||*ptr=='z')
	    {
	      subtype = *ptr++;
	      if (*ptr == subtype)
		{
		  subtype=toupper(subtype);
		  ptr++;
		}
	    }

	  type=*ptr++;

	  if(type&&type!='%'&&type!='c'&&type!='n'&&type!='[')
	    { do /* ignore leading whitespace characters */
		NEXT(c);
	      while(isspace(c));
	      ms.size=1;
	    } /* The first non-whitespace character is already read */

	  switch(type)
	    { case 'c':
		{ 
		  void *dest = NULL;

		  if(width==ULONG_MAX) /* Default */
		    width=1;
		
		  if(!ignore)
		    {
		      dest = va_arg(args, void *);
		    }

		  NEXT(c);  /* 'c' did not skip space */
		  get_string( subtype == WCHAR_SUBTYPE,
			      dest, width, c, stream, ignore, ms_ptr, NULL, 0 );
		  if(!ignore&&ms.size)
		    blocks++;
		  break;
		}
	    case '[':
	      {
		void *dest = NULL;
		const unsigned char *scanset;

		scanset = (const unsigned char *)ptr;
		if(*ptr=='^')
		    ptr++;
		if (*ptr==']')
		  ptr++;
		while (*ptr && *ptr != ']')
		  ptr++;
		if (*ptr) ptr++;

		if(!ignore)
		  dest=va_arg(args,void *);

		NEXT(c);
		get_string( subtype == WCHAR_SUBTYPE,
			    dest, width, c, stream, ignore, ms_ptr, scanset, 1 );
		if(!ignore&&ms.size)
		  {
		    blocks++;
		  }
		break;
	      }
	    case 's':
	      { void *destp = NULL;

		if(!ignore)
		  {
		    destp=va_arg(args,void *);
		  }
		get_string( subtype == WCHAR_SUBTYPE,
			    destp, width, c, stream, ignore, ms_ptr,
			    (unsigned char *)"^ \t\r\n\f\v]",
			    1 );

		if(!ignore&&ms.size)
		  { 
		    blocks++;
		  }
		break;
	      }
#ifdef FLOAT_SUPPORT
	    case 'e':
	    case 'f':
	    case 'g':
	      { long double v;
		int ex=0;
		int min=0,mine=0; /* This is a workaround for gcc 2.3.3: should be char */
		
		do /* This is there just to be able to break out */
		  {
		    if(VAL(c=='-'||c=='+'))
		      { min=c;
			NEXT(c); }

		    if(VAL(tolower(c)=='i')) /* +- inf */
		      { int d;
			NEXT(d);
			if(VAL(tolower(d)=='n'))
			  { int e;
			    NEXT(e);
			    if(VAL(tolower(e)=='f'))
			      { v=HUGE_VAL;
				if (min=='-') v=-v;
				break; } /* break out */
			    PREV(e);
			  }
			PREV(d);
		      }
		    else if(VAL(toupper(c)=='N')) /* NaN */
		      { int d;
			NEXT(d);
			if(VAL(tolower(d)=='a'))
			  { int e;
			    NEXT(e);
			    if(VAL(toupper(e)=='N'))
			      { v=_NAN;
				break; }
			    PREV(e);
			  }
			PREV(d);
		      }

		    v=0.0;
		    while(VAL(isdigit(c)))
		      { v=v*10.0+(c-'0');
			NEXT(c);
		      }

		    if(VAL(c=='.'))
		      { long double dp=0.1;
			NEXT(c);
			while(VAL(isdigit(c)))
			  { v=v+dp*(c-'0');
			    dp=dp/10.0;
			    NEXT(c); }
			if(ms.size==2+(min!=0)) /* No number read till now -> malformatted */
			  { PREV(c);
			    c='.';
			  }
		      }

		    if(min&&ms.size==2) /* No number read till now -> malformatted */
		      { PREV(c);
			c=min;
		      }
		    if(ms.size==1)
		      break;

		    if(VAL(tolower(c)=='e'))
		      { int d;
			NEXT(d);
			if(VAL(d=='-'||d=='+'))
			  { mine=d;
			    NEXT(d);
			  }

			if(VAL(isdigit(d)))
			  { do
			      { ex=ex*10+(d-'0');
				NEXT(d);
			      }while(VAL(isdigit(d)&&ex<100));
			    c=d;
			  } else
			  { PREV(d);
			    if(mine)
			      PREV(mine);
			  }
		      }
		    PREV(c);

		    if(mine=='-')
		      v=v/pow(10.0,ex);
		    else
		      v=v*pow(10.0,ex);

		    if(min=='-')
		      v=-v;

		  }while(0);

		if(!ignore&&ms.size)
		  { switch(subtype)
		      {
		      case 'L':
			*va_arg(args,long double*)=v;
			break;
		      case 'l':
			*va_arg(args,double *)=v;
		      break;
		      case 'i':
		      default:
			*va_arg(args,float *)=v;
			break;
		      }
		    blocks++;
		  }
		break;
	      }
#endif
	    case '%':
	      NEXT(c);
	      if(c!='%')
		PREV(c); /* unget non-'%' character */
	      break;
	    case 'n':
	      if(!ignore)
		*va_arg(args,int *)=ms.incount;
	      ms.size=1; /* fake a valid argument */
	      blocks++;
	      break;
	    default:
	      { ULongLong v=0;
		int base;
		int min=0;

		if(!type)
		  ptr--; /* unparse NUL character */

		if(type=='p')
		  { subtype='l'; /* This is the same as %lx */
		    type='x'; }

		if(VAL((c=='-'&&type!='u')||c=='+'))
		  { min=c;
		    NEXT(c); }

		if (VAL(c=='0')&&(type=='x'||type=='i'))
		  {
		    if (type=='i')
		      type='o';
		    NEXT(c);
		    if (VAL(c=='x')) {
		      ms.size = 0;
		      type='x';
		      NEXT(c);
		    }
		  }

		base=type=='x'||type=='X'?16:(type=='o'?8:10);
		while(VAL(isxdigit(c)&&(base!=10||isdigit(c))&&(base!=8||c<='7')))
		  { v=v*base+(isdigit(c)?c-'0':0)+(isupper(c)?c-'A'+10:0)+(islower(c)?c-'a'+10:0);
		    NEXT(c);
		  }

		if(min&&ms.size==2) /* If there is no valid character after sign, unget last */
		  { PREV(c);
		    c=min; }

		PREV(c);

		if(ignore||!ms.size)
		  break;

		if(type=='u')
		  switch(subtype)
		    {
#ifdef LONGLONG_SUPPORT                        
		    case 'L':
		      *va_arg(args,unsigned long long *)=v;
		      break;
#endif                      
		    case 'l':
		    case 'z':
		    case 'j':
		    case 't':
		      *va_arg(args,unsigned long *)=v;
		      break;
		    case 'i':
		      *va_arg(args,unsigned int *)=v;
		      break;
		    case 'h':
		      *va_arg(args,unsigned short *)=v;
		      break;
		    case 'H':
		      *va_arg(args,unsigned char *)=v;
		      break;
		    }
		else
		  { SLongLong v2;
		    if(min=='-')
		      v2=-v;
		    else
		      v2=v;
		    switch(subtype)
		      {
#ifdef LONG_LONG_SUPPORT                          
		      case 'L':
			*va_arg(args,signed long long *)=v2;
		        break;
#endif                        
		      case 'l':
		      case 'z':
		      case 'j':
		      case 't':
			*va_arg(args,signed long *)=v2;
		        break;
		      case 'i':
			*va_arg(args,signed int *)=v2;
			break;
		      case 'h':
			*va_arg(args,signed short *)=v2;
			break;
		      case 'H':
			*va_arg(args,signed char *)=v2;
			break;
		      }
		  }
		blocks++;
		break;
	      }
	    }
	  format_ptr=(const char *)ptr;
	  fmt = *format_ptr++;
	}else
	{ if(isspace(fmt))
	    { do
		NEXT(c);
	      while(isspace(c));
	      PREV(c);
	      ms.size=1; }
	  else
	    { NEXT(c);
	      if(c!=fmt)
		PREV(c); }
	  fmt = *format_ptr++;
	}
      if(!ms.size)
	break;
    }

  if(c==EOF&&!blocks)
    return c;
  else
    return blocks;
}
