#include "math.h"

float magnitude(float x, float y)
{
    return sqrtf(x * x + y * y);
}

fixed_t fixedmul(fixed_t a, fixed_t b)
{
    int64_t a64, b64;

    a64 = a;
    b64 = b;

    return (a64 * b64) >> FIXEDSHIFT;
}