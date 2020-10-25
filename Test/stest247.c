#define FIXED_POINT 16

typedef int Real;

Real toReal(float a)
{
    return (int)(a * (float)(1<<FIXED_POINT));
}
