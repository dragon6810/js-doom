#ifndef _DRAW_H
#define _DRAW_H

#include "doommath.h"
#include "wad.h"

void draw_postcolumn(post_t* post, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale);

#endif