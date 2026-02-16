#ifndef _SCREEN_H
#define _SCREEN_H

#include <stdint.h>

void screen_init(int width, int height);

extern int screenwidth;
extern int screenheight;

extern uint32_t *pixels;

#endif