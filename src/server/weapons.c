#include "weapon.h"

#include "level.h"
#include "lineatk.h"
#include "player.h"
#include "snd.h"

void A_FirePistol()
{
    int edict;
    float slope;

    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    edict = (int)(curwpnplayer->mobj - mobjs);
    snd_queueedict(SFX_PISTOL, edict);
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);

    slope = lineatk_findslope(curwpnplayer->mobj, curwpnplayer->mobj->info.angle);
    printf("atk slope: %f\n", slope);
}

void A_FireShotgun()
{
    int edict;

    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    edict = (int)(curwpnplayer->mobj - mobjs);
    snd_queueedict(SFX_SHOTGN, edict);
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);
}
