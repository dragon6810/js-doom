#include "menu.h"

#include <string.h>

#include "draw.h"

#define NSKULLFRAMES 2
#define TEXTHEIGHT 7

lumpinfo_t *skullframes[NSKULLFRAMES] = {};
lumpinfo_t *textlumps[256] = {};

void menu_init(void)
{
    int i;

    char name[9];

    strcpy(name, "M_SKULL ");
    for(i=0; i<NSKULLFRAMES; i++)
    {
        name[7] = i + '1';
        skullframes[i] = wad_findlump(name, true);
    }

    for(i=0; i<256; i++)
    {
        snprintf(name, 9, "STCFN%03d", i);
        textlumps[i] = wad_findlump(name, false);
    }
}

static int menu_gettextwidth(const char* text)
{
    const char *c;
    
    int w;

    if(!text)
        return 0;

    for(c=text, w=0; *c; c++)
    {
        if(!textlumps[*c])
            continue;

        wad_cache(textlumps[*c]);
        w += ((pic_t*)textlumps[*c]->cache)->w;
    }

    return w;
}

static int menu_getbtnheight(menuel_button_t* btn)
{
    if(btn->lump)
    {
        wad_cache(btn->lump);
        return ((pic_t*)btn->lump->cache)->h;
    }
    
    return TEXTHEIGHT;
}

static int menu_getbtnwidth(menuel_button_t* btn)
{
    if(btn->lump)
    {
        wad_cache(btn->lump);
        return ((pic_t*)btn->lump->cache)->w;
    }
    else
        return menu_gettextwidth(btn->text);
}

static int menu_getelheight(menuel_t* menuel)
{
    switch(menuel->type)
    {
    case MENUEL_BUTTON:
        return menu_getbtnheight(&menuel->button);
    default:
        return 0;
    }
}

static int menu_getelleft(menuel_t* menuel)
{
    int w;

    switch(menuel->type)
    {
    case MENUEL_BUTTON:
        w = menu_getbtnwidth(&menuel->button);
        break;
    default:
        w = 0;
        break;
    }

    switch(menuel->xalign)
    {
    case ALIGN_CENTER:
        return menuel->x - w / 2;
    case ALIGN_RIGHT:
        return menuel->x - w;
    default:
        return menuel->x;
    }
}

static void menu_drawtext(int x, int y, const char* text)
{
    const char *c;

    if(!text)
        return;

    for(c=text; *c; c++)
    {
        if(!textlumps[*c])
            continue;
        wad_cache(textlumps[*c]);
        draw_scaledpic(textlumps[*c]->cache, NULL, colormap->maps[0], x << FIXEDSHIFT, y << FIXEDSHIFT);
        x += ((pic_t*)textlumps[*c]->cache)->w;
    }
}

static void menu_drawel(menuel_t* el)
{
    int left, top;

    left = menu_getelleft(el);
    top = el->y - ceilf(menu_getelheight(el) / 2.0);

    switch(el->type)
    {
    case MENUEL_BUTTON:
        if(el->button.lump)
        {
            wad_cache(el->button.lump);
            draw_scaledpic(el->button.lump->cache, NULL, colormap->maps[0], left << FIXEDSHIFT, top << FIXEDSHIFT);
            break;
        }

        menu_drawtext(left, top, el->button.text);
        break;
    default:
        break;
    }
}

void menu_draw(menu_t* menu, float time)
{
    int i;

    int curfame;
    int skullframe, skullx, skully;

    for(i=0; i<menu->nels; i++)
    {
        menu_drawel(&menu->els[i]);
        if(i != menu->selection)
            continue;

        skullframe = time * 35.0 / 8.0;
        skullframe &= 1;

        wad_cache(skullframes[skullframe]);
        skully = menu->els[i].y + menu_getelheight(&menu->els[i]) / 2 - ((pic_t*)skullframes[skullframe]->cache)->h;
        skullx = menu_getelleft(&menu->els[i]) - 12 - ((pic_t*)skullframes[skullframe]->cache)->w;

        draw_scaledpic(skullframes[skullframe]->cache, NULL, colormap->maps[0], skullx << FIXEDSHIFT, skully << FIXEDSHIFT);
    }
}

void menu_input(menu_t* menu, bool select, bool press)
{

}