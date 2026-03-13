#ifndef _DOOMMATH_H
#define _DOOMMATH_H

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define ANG360 0xFFFFFFFFu
#define ANG270 (ANG180 + ANG90)
#define ANG180 0x80000000u
#define ANG90  0x40000000u
#define ANG60  (ANG180 / 3)
#define ANG45  0x20000000u
#define ANG30  (ANG60 >> 1)
#define ANG15  (ANG30 >> 1)
#define ANG1   (ANG15 / 15)

#define BAM_SCALE ((uint64_t) UINT32_MAX + 1)

#define TABANGLES 8192
#define TABSHIFT 19

#define ANGTORAD(ang) ((float) ((double) (ang) / (double) BAM_SCALE * 2.0 * (double) M_PI))
#define ANGTODEG(ang) ((float) ((double) (ang) / (double) BAM_SCALE * 360.0))
#define RADTOANG(rad) ((angle_t) (sangle_t) ((double) (rad) / (2.0 * M_PI) * (double) BAM_SCALE))
#define DEGTOANG(deg) ((angle_t) (sangle_t) ((double) (deg) / 360.0 * (double) BAM_SCALE))

#define ANGSIN(ang) sin(ANGTORAD(ang))
#define ANGCOS(ang) cos(ANGTORAD(ang))
#define ANGTAN(ang) tan(ANGTORAD(ang))
#define ANGATAN(val) RADTOANG(atan(val))
#define ANGATAN2(y, x) RADTOANG(atan2(y, x))

#define FIXEDSHIFT 16
#define FIXEDUNIT (1 << FIXEDSHIFT)
#define FLOATTOFIXED(f) ((fixed_t) ((f) * (float) ((fixed_t) 1 << FIXEDSHIFT)))
#define FIXEDTOFLOAT(fixed) ((float) (fixed) / (float) ((fixed_t) 1 << FIXEDSHIFT))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) MAX(MIN(x, max), min)
#define LERP(a, b, t) (((b) - (a)) * (t) + (a))
// inclusive
#define INRANGE(x, min, max) ((x) >= (min) && (x) <= (max))

typedef uint32_t angle_t;
typedef int32_t sangle_t;
typedef int32_t fixed_t;

float magnitude(float x, float y);
fixed_t fixedmag(fixed_t x, fixed_t y);
fixed_t fixedmul(fixed_t a, fixed_t b);
fixed_t fixeddiv(fixed_t a, fixed_t b);

// returns t along line 1 where it hits seg 2
// t can be < 0 or > 1 but that means its not on the segment
// INFINITY means line 1 didnt touch seg 2
float segmentsegment(float v1x1, float v1y1, float v1x2, float v1y2, float v2x1, float v2y1, float v2x2, float v2y2);
// returns t along line where it hits square
// t can be < 0 or > 1 but that means its not on the segment
// INFINITY means line didnt touch square
float segmentsquare(float x1, float y1, float x2, float y2, float x, float y, float radius);
bool boxbox(float xmin1, float ymin1, float xmax1, float ymax1, float xmin2, float ymin2, float xmax2, float ymax2);

#endif