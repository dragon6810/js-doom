#ifndef _LINEATK_H
#define _LINEATK_H

#include "level.h"

float lineatk_findslope(object_t* mobj, angle_t ang);
bool lineatk(int dmg, object_t* mobj, angle_t ang, float dist, float slope);

#endif