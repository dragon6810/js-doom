#ifndef _SPECIAL_H
#define _SPECIAL_H

#include "level.h"

bool special_doorsec(object_t* user, sector_t* sec, int special);
bool special_door(object_t* user, linedef_t* line, int special);

#endif