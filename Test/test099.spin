#undef  R0
#define R1 99
#undef  R2

#ifdef R0
# define R R0
#elseifndef R1
#error bad def
#elseifdef R1
# define R R1
#elseifdef R2
# define R R2
#else
# define R (-1)
#endif

PUB blah(x)
  return R

