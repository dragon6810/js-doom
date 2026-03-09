#include "visweapon.h"

#include "draw.h"
#include "player.h"
#include "render.h"
#include "wad.h"

void visweapon_draw(float progtime)
{
    int sectorlight, level;
    lumpinfo_t *lump;
    float bob, xoffs, yoffs;
    angle_t angle;

    bob = player_calcbobamp(player.mobj);

    angle = (angle_t)(128.0 * progtime * 35.0) << (32 - 13);

    sectorlight = 15;
    if(player.mobj->ssector)
    {
        sectorlight = player.mobj->ssector->sector->light >> LIGHTSHIFT;
        sectorlight = CLAMP(sectorlight, 0, LIGHTLEVELS - 1);
    }

    xoffs = bob * ANGCOS(angle);
    yoffs = 16 + bob * ANGSIN(angle & (ANG180 - 1));
    if(player.info.weapon.state == wpndefs[player.info.weapon.cur].upst)
        yoffs += player.info.weapon.time * (6.0*35.0);
    else if(player.info.weapon.state == wpndefs[player.info.weapon.cur].downst)
        yoffs += 128 - player.info.weapon.time * (6.0*35.0);
    
    lump = &lumps[sprites[states[player.info.weapon.state].sprite].frames[states[player.info.weapon.state].frame].rotlumps[0]];
    wad_cache(lump);
    draw_scaledpic(lump->cache, NULL, scalemap[sectorlight][SCALEBANDS-1], FLOATTOFIXED(xoffs), FLOATTOFIXED(yoffs));
}