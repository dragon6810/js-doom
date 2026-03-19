#ifndef _MENU_H
#define _MENU_H

#include "wad.h"

typedef enum
{
    ALIGN_LEFT=0,
    ALIGN_CENTER,
    ALIGN_RIGHT,
} align_e;

typedef enum
{
    MENUEL_BUTTON,

    NUM_MENUEL
} menuel_e;

typedef struct
{
    lumpinfo_t *lump; // if NULL, use text instead
    const char *text;

    void (*callback)(void);
} menuel_button_t;

typedef struct
{
    int x;
    int y; // centered

    align_e xalign;
    
    menuel_e type;
    union
    {
        menuel_button_t button;
    };
} menuel_t;

typedef struct
{
    int nels;
    menuel_t *els;

    int selection;
} menu_t;

void menu_init(void);
void menu_draw(menu_t* menu, float time);
void menu_input(menu_t* menu, bool select, bool press);

#endif