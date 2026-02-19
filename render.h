#ifndef _RENDER_H
#define _RENDER_H

#include "math.h"
#include "screen.h"

#define HFOV ANG90
#define HPLANE (ANGTAN(HFOV>>1)*2.0)
#define VPLANE (HPLANE*(float)screenheight/(float)screenwidth)

extern float viewx, viewy, viewz;
extern angle_t viewangle;

void render_init(void);
void render(void);

#endif