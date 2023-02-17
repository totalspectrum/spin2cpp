#include <stdlib.h>

double
atof(const char *str)
{
#ifdef __FLEXC__
    return __builtin_atof(str);
#else
    return strtod(str, NULL);
#endif
}
