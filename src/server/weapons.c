#include "weapon.h"

#include "level.h"
#include "player.h"
#include "snd.h"

void A_FirePistol()
{
    int edict;

    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    edict = (int)(curwpnplayer->mobj - mobjs);
    snd_queueedict(SFX_PISTOL, edict);
    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);
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
