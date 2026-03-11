#include "weapon.h"

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
    snd_queueedict(SFX_PISTOL, edict);
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);

    angle = curwpnplayer->mobj->info.angle;
    slope = lineatk_findslope(curwpnplayer->mobj, curwpnplayer->mobj->info.angle);
    if(refiring)
        angle += ((prand() - prand()) << 18);
    lineatk(5 * (prand() % 3 + 1), curwpnplayer->mobj, angle, 2048, slope);
}

void A_FireShotgun()
{
    int i;
    float slope;

    int edict;

    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    edict = (int)(curwpnplayer->mobj - mobjs);
    snd_queueedict(SFX_SHOTGN, edict);
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);

    slope = lineatk_findslope(curwpnplayer->mobj, curwpnplayer->mobj->info.angle);
    for(i=0; i<7; i++)
        lineatk(5 * (prand() % 3 + 1), curwpnplayer->mobj, curwpnplayer->mobj->info.angle + ((prand() - prand()) << 18), 2048, slope);
}
