#include "visweapon.h"

#include "draw.h"
#include "player.h"
#include "wad.h"

void visweapon_draw(float progtime)
{
    lumpinfo_t *lump;
    float bob, xoffs, yoffs;
    angle_t angle;

    bob = player_calcbobamp(player.mobj);

    angle = (angle_t)(128.0 * progtime * 35.0) << (32 - 13);

    xoffs = bob * ANGCOS(angle);
    yoffs = 16 + bob * ANGSIN(angle & (ANG180 - 1));
    
    lump = &lumps[sprites[states[S_PISTOL].sprite].frames[states[S_PISTOL].frame].rotlumps[0]];
    wad_cache(lump);
    draw_pic(lump->cache, colormap->maps[0], xoffs, yoffs);
}