#ifndef _DRAW_H
#define _DRAW_H

#include "doommath.h"
#include "level.h"
#include "tex.h"
#include "wad.h"

extern uint8_t transtbls[NUM_MC][256];

void draw_init(void);
void draw_pic(pic_t* pic, uint8_t* colmap, int x, int y);
void draw_postcolumn(post_t* post, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale);
void draw_transpostcolumn(post_t* post, uint8_t* trans, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale);
void draw_texcolumn(uint8_t* col, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale);

#endif