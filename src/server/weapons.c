#include "weapon.h"

#include <assert.h>
#include <string.h>

#include "level.h"
#include "lineatk.h"
#include "player.h"
#include "rand.h"
#include "snd.h"

void A_FirePistol()
{
    int edict;
    float slope;
    angle_t angle;

    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    edict = (int)(curwpnplayer->mobj - mobjs);
    snd_queueedict(sfx_pistol, edict);
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);

    angle = curwpnplayer->mobj->info.angle;
    slope = lineatk_findslope(curwpnplayer->mobj, curwpnplayer->mobj->info.angle);
    if(refiring)
        angle += ((prand() - prand()) << 18);
    lineatk(5 * (prand() % 3 + 1), curwpnplayer->mobj, angle, 2048, slope);

    curwpnplayer->info.ammo[wpndefs[WEAPON_PIST].ammo]--;
}

void A_FireShotgun()
{
    int i;
    float slope;

    int edict;

    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    edict = (int)(curwpnplayer->mobj - mobjs);
    snd_queueedict(sfx_shotgn, edict);
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);

    slope = lineatk_findslope(curwpnplayer->mobj, curwpnplayer->mobj->info.angle);
    for(i=0; i<7; i++)
        lineatk(5 * (prand() % 3 + 1), curwpnplayer->mobj, curwpnplayer->mobj->info.angle + ((prand() - prand()) << 18), 2048, slope);

    curwpnplayer->info.ammo[wpndefs[WEAPON_SHOT].ammo]--;
}

void A_FireCGun()
{
    int edict;
    float slope;
    angle_t angle;

    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    edict = (int)(curwpnplayer->mobj - mobjs);
    snd_queueedict(sfx_pistol, edict);
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);

    angle = curwpnplayer->mobj->info.angle;
    slope = lineatk_findslope(curwpnplayer->mobj, curwpnplayer->mobj->info.angle);
    if(refiring)
        angle += ((prand() - prand()) << 18);
    lineatk(5 * (prand() % 3 + 1), curwpnplayer->mobj, angle, 2048, slope);

    curwpnplayer->info.ammo[wpndefs[WEAPON_CHAIN].ammo]--;
}

static void spawnrocket(angle_t angle, float slope)
{
    int edict;
    object_t *mobj;

    edict = level_findnewedict();
    assert(edict >= 0);
    mobj = &mobjs[edict];

    memset(mobj, 0, sizeof(object_t));
    mobj->info.exists = true;
    mobj->info.type = MT_ROCKET;
    mobj->info.x = curwpnplayer->mobj->info.x + ANGCOS(angle) * 32;
    mobj->info.y = curwpnplayer->mobj->info.y + ANGSIN(angle) * 32;
    mobj->info.z = curwpnplayer->mobj->info.z + 32;
    mobj->info.angle = angle;
    mobj->info.health = mobjinfo[MT_ROCKET].spawnhealth;
    mobj->info.flags = mobjinfo[MT_ROCKET].flags;
    mobj->info.height = mobjinfo[MT_ROCKET].height;

    mobj->info.state = mobjinfo[MT_ROCKET].spawnstate;
    if(mobjinfo[MT_ROCKET].seestate)
        snd_queueedict(mobjinfo[MT_ROCKET].seestate, edict);

    mobj->info.xvel = ANGCOS(angle) * mobjinfo[MT_ROCKET].speed * 35.0;
    mobj->info.yvel = ANGSIN(angle) * mobjinfo[MT_ROCKET].speed * 35.0;
    mobj->info.zvel = slope * mobjinfo[MT_ROCKET].speed * 35.0;

    mobj->target = curwpnplayer->mobj;
    
    level_placemobj(mobj);
    level_addmobjthinker(mobj);
}

void A_FireMissile()
{
    int edict;
    float slope;
    angle_t angle;

    if(!curwpnplayer || !curwpnplayer->mobj)
        return;
    
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);

    angle = curwpnplayer->mobj->info.angle;
    slope = lineatk_findslope(curwpnplayer->mobj, curwpnplayer->mobj->info.angle);
    
    spawnrocket(angle, slope);

    curwpnplayer->info.ammo[wpndefs[WEAPON_ROCKET].ammo]--;
}
