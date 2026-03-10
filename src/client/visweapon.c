#include "visweapon.h"

#include "connection.h"
#include "draw.h"
#include "player.h"
#include "render.h"
#include "snd.h"
#include "wad.h"

statenum_t flashstate = S_NULL;
float flashtime = 0;

bool weaponprediction = false;

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

    lump = &lumps[sprites[states[player.info.weapon.state].sprite].frames[states[player.info.weapon.state].frame&0x7FFF].rotlumps[0]];
    wad_cache(lump);
    draw_scaledpic(lump->cache, NULL, scalemap[sectorlight][SCALEBANDS-1], FLOATTOFIXED(xoffs), FLOATTOFIXED(yoffs));

    if(flashstate != S_NULL)
    {
        lump = &lumps[sprites[states[flashstate].sprite].frames[states[flashstate].frame&0x7FFF].rotlumps[0]];
        wad_cache(lump);
        draw_scaledpic(lump->cache, NULL, colormap->maps[0], FLOATTOFIXED(xoffs), FLOATTOFIXED(yoffs));
    }
}

void visweapon_tick(float ft)
{
    if(flashstate != S_NULL && !weaponprediction)
    {
        flashtime += ft;
        while(flashstate != S_NULL && flashtime >= states[flashstate].tics / 35.0)
        {
            flashtime -= states[flashstate].tics / 35.0;
            flashstate = states[flashstate].nextstate;
        }
    }
}

void A_FirePistol()
{
    if(weaponprediction)
        return;

    flashstate = S_PISTOLFLASH;
    flashtime = 1.0/35.0;
    snd_playsoundedict(SFX_PISTOL, serverconn.edict);
}

void A_FireShotgun()
{
    if(weaponprediction)
        return;

    flashstate = S_SGUNFLASH1;
    flashtime = 1.0/35.0;
    snd_playsoundedict(SFX_SHOTGN, serverconn.edict);
}