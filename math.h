#include <limits.h>
#include <math.h>
#include <stdint.h>

#define ANG270 (ANG180 + ANG90)
#define ANG180 0x80000000u
#define ANG90  0x40000000u
#define ANG60  (ANG180 / 3)
#define ANG45  0x20000000u
#define ANG30  (ANG60 >> 1)
#define ANG15  (ANG30 >> 1)
#define ANG1   (ANG15 / 15)

#define BAM_SCALE ((uint64_t) UINT32_MAX + 1)

#define ANGTORAD(ang) ((float) ((double) (ang) / (double) BAM_SCALE * 2.0 * (double) M_PI))
#define ANGTODEG(ang) ((float) ((double) (ang) / (double) BAM_SCALE * 360.0))
#define RADTOANG(rad) ((angle_t) (sangle_t) ((double) (rad) / (2.0 * M_PI) * (double) BAM_SCALE))
#define DEGTOANG(deg) ((angle_t) (sangle_t) ((double) (deg) / 360.0 * (double) BAM_SCALE))

#define ANGSIN(ang) sin(ANGTORAD(ang))
#define ANGCOS(ang) cos(ANGTORAD(ang))
#define ANGTAN(ang) tan(ANGTORAD(ang))
#define ANGATAN2(y, x) RADTOANG(atan2(y, x))

typedef uint32_t angle_t;
typedef int32_t sangle_t;