#ifndef _VISWEAPON_H
#define _VISWEAPON_H

#include <stdbool.h>

#include "info.h"

extern statenum_t flashstate;
extern float flashtime;

extern bool weaponprediction;

void visweapon_draw(float progtime);
void visweapon_tick(float ft);

#endif