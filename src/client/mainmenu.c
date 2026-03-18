#include "mainmenu.h"

#include "draw.h"
#include "wad.h"

lumpinfo_t *backgroundlump = NULL;

void mainmenu_findlumps(void)
{
    backgroundlump = wad_findlump("TITLEPIC", true);
}

void mainmenu_draw(void)
{
    drawscreen = &screens[SCR_MENU];
    
    if(backgroundlump)
    {
        wad_cache(backgroundlump);
        draw_scaledpic(backgroundlump->cache, NULL, colormap->maps[0], 0, 0);
    }
}