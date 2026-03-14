#include "doommath.h"

#include <stdlib.h>

float magnitude(float x, float y)
{
    return sqrtf(x * x + y * y);
}

fixed_t fixedmag(fixed_t x, fixed_t y)
{
    angle_t angle;
    fixed_t num, den;

    angle = ANGATAN2(y, x);
    
    num = y;
    den = FLOATTOFIXED(ANGSIN(angle));
    if(abs(den) < 1 << (FIXEDSHIFT - 8))
        return abs(x);

    return fixeddiv(num, den);
}

fixed_t fixedmul(fixed_t a, fixed_t b)
{
    int64_t a64, b64;

    a64 = a;
    b64 = b;

    return (a64 * b64) >> FIXEDSHIFT;
}

fixed_t fixeddiv(fixed_t a, fixed_t b)
{
    if((abs(a) >> (FIXEDSHIFT - 2)) >= abs(b))
	    return (a ^ b) < 0 ? INT_MIN : INT_MAX;

	return ((int64_t) a << FIXEDSHIFT) / b;
}

float segmentsegment(float v1x1, float v1y1, float v1x2, float v1y2, float v2x1, float v2y1, float v2x2, float v2y2)
{
    float dx1, dy1, dx2, dy2;
    float rx, ry;
    float denom, t, s;

    dx1 = v1x2 - v1x1;
    dy1 = v1y2 - v1y1;
    dx2 = v2x2 - v2x1;
    dy2 = v2y2 - v2y1;

    denom = dx1 * dy2 - dy1 * dx2;
    if(denom == 0)
        return INFINITY;

    rx = v2x1 - v1x1;
    ry = v2y1 - v1y1;

    t = (rx * dy2 - ry * dx2) / denom;
    s = (rx * dy1 - ry * dx1) / denom;

    if(s < 0 || s > 1)
        return INFINITY;

    return t;
}

float segmentsquare(float x1, float y1, float x2, float y2, float x, float y, float radius)
{
    float t, mint;

    mint = INFINITY;

    t = segmentsegment(x1, y1, x2, y2, x - radius, y - radius, x - radius, y + radius);
    if(t < mint) mint = t;

    t = segmentsegment(x1, y1, x2, y2, x + radius, y - radius, x + radius, y + radius);
    if(t < mint) mint = t;

    t = segmentsegment(x1, y1, x2, y2, x - radius, y - radius, x + radius, y - radius);
    if(t < mint) mint = t;

    t = segmentsegment(x1, y1, x2, y2, x - radius, y + radius, x + radius, y + radius);
    if(t < mint) mint = t;

    return mint;
}

bool boxbox(float xmin1, float ymin1, float xmax1, float ymax1, float xmin2, float ymin2, float xmax2, float ymax2)
{
    if(xmin2 > xmax1)
        return false;
    if(xmin1 > xmax2)
        return false;
    if(ymin2 > ymax1)
        return false;
    if(ymin1 > ymax2)
        return false;
    return true;
}
