#include "weapon.h"

#include "player.h"

void A_FirePistol()
{
    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);
}

void A_FireShotgun()
{
    if(!curwpnplayer || !curwpnplayer->mobj)
        return;

    level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK2);
}
