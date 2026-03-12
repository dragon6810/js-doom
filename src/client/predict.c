#include "predict.h"

#include <string.h>

#include "connection.h"
#include "visweapon.h"

playercmd_t inputwindow[PRED_WINDOW] = {};
gamestate_t oldgs = {};
gamestate_t newgs = {};
wpnst_t startwpn = {};

void predictplayer(void)
{
    int i;

    int start, end;

    player.info.weapon = startwpn;

    level_unplacemobj(&mobjs[serverconn.edict]);
    mobjs[serverconn.edict].info = newgs.mobjs[serverconn.edict];
    level_placemobj(&mobjs[serverconn.edict]);

    start = serverconn.chan.inack + 1;
    end = serverconn.chan.outseq;

    if(end - start >= PRED_WINDOW)
        return;

    curwpnplayer = &player;

    weaponprediction = true;
    for(i=start; i<=end; i++)
    {
        player.lastcmd = inputwindow[i % PRED_WINDOW];
        player_docmd(&player, &inputwindow[i % PRED_WINDOW]);
        weapon_tickstate(&player.info.weapon, inputwindow[i % PRED_WINDOW].frametime);
        visweapon_tick(inputwindow[i % PRED_WINDOW].frametime);
    }
}

void interpent(float t, int edict)
{
    objinfo_t *obj, *old, *new;

    obj = &mobjs[edict].info;
    old = &oldgs.mobjs[edict];
    new = &newgs.mobjs[edict];

    *obj = *new;
    obj->x = LERP(old->x, new->x, t);
    obj->y = LERP(old->y, new->y, t);
    obj->z = LERP(old->z, new->z, t);

    level_unplacemobj(&mobjs[edict]);
    level_placemobj(&mobjs[edict]);
}

void interpsectors(float abstime)
{
    int i;
    float t;

    if(!oldgs.sectorinfos || !newgs.sectorinfos)
        return;

    t = (abstime - lastrecvtime) * TICRATE;
    t = CLAMP(t, 0, 1);

    for(i=0; i<nsectors; i++)
    {
        sectors[i].floorheight = LERP(oldgs.sectorinfos[i].floorheight, newgs.sectorinfos[i].floorheight, t);
        sectors[i].ceilheight = LERP(oldgs.sectorinfos[i].ceilheight, newgs.sectorinfos[i].ceilheight, t);
    }
}

void interpentities(float abstime)
{
    int i;

    float interptime;

    interptime = (abstime - lastrecvtime) * TICRATE;
    interptime = CLAMP(interptime, 0, 1);

    for(i=0; i<=newgs.maxmobj; i++)
    {
        if(i == serverconn.edict)
            continue;

        if(!newgs.mobjs[i].exists)
        {
            level_unplacemobj(&mobjs[i]);
            memset(&mobjs[i].info, 0, sizeof(objinfo_t));
            continue;
        }

        if(!oldgs.mobjs[i].exists)
        {
            level_unplacemobj(&mobjs[i]);
            mobjs[i].info = newgs.mobjs[i];
            level_placemobj(&mobjs[i]);
            continue;
        }

        interpent(interptime, i);
    }
}
