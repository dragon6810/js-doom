#include "visweapon.h"

#include "draw.h"
#include "wad.h"

void visweapon_draw(void)
{
    lumpinfo_t *lump;
    
    lump = &lumps[sprites[states[S_SGUN].sprite].frames[states[S_SGUN].frame].rotlumps[0]];
    wad_cache(lump);
    draw_pic(lump->cache, colormap->maps[0], 0, 0);
}