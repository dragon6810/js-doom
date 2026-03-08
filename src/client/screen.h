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

void screen_initscreens(void);
void screen_present(void);
void screen_setpal(color_t* pal);

#endif