#ifndef _RENDER_H
#define _RENDER_H

#include "doommath.h"
#include "screen.h"

#define HFOV ANG90
#define HPLANE (ANGTAN(HFOV>>1)*2.0)
#define VPLANE (HPLANE*(float)screenheight/(float)screenwidth)

#define LIGHTLEVELS 16
#define LIGHTSHIFT 4 // 0-255 to 0-15
#define SCALEBANDS 48

extern float viewx, viewy, viewz;
extern angle_t viewangle;

extern int rectheight;

extern uint8_t *scalemap[LIGHTLEVELS][SCALEBANDS];

void render_init(void);
void render(float progtime);

#endif