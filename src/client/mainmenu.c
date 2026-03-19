#include "mainmenu.h"

#include "draw.h"
#include "menu.h"
#include "wad.h"

lumpinfo_t *backgroundlump = NULL;
menuel_t mainmenuels[] = 
{
    { .x = 160, .y = 100, .xalign = ALIGN_CENTER, .type = MENUEL_BUTTON, .button.lump = NULL, .button.callback = NULL },
};
menu_t mainmenu = { 1, &mainmenuels, 0 };

void mainmenu_findlumps(void)
{
    backgroundlump = wad_findlump("TITLEPIC", true);
    mainmenuels[0].button.lump = wad_findlump("M_NGAME", true);
}

void mainmenu_draw(float time)
{
    drawscreen = &screens[SCR_MENU];
    
    if(backgroundlump)
    {
        wad_cache(backgroundlump);
        draw_scaledpic(backgroundlump->cache, NULL, colormap->maps[0], 0, 0);
    }

    menu_draw(&mainmenu, time);
}