#ifndef _SCREEN_H
#define _SCREEN_H

#include <stdint.h>

#include "wad.h"

typedef enum
{
    SCR_LVL=0,
    SCR_ST,

    NUM_SCR,
} screen_e;

typedef struct
{
    int w, h;
    int x, y;
    uint8_t* pixels;
} screen_t;

extern int screenwidth;
extern int screenheight;

extern screen_t screens[NUM_SCR];

extern color_t *curpalette;

void screen_initscreens(void);
void screen_present(void);

#endif