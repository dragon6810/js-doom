#include "special.h"

#include <assert.h>
#include <stdlib.h>

#include "player.h"
#include "snd.h"
#include "think.h"

typedef struct
{
    thinker_t thinker;
    int state; // 1 -> raising, 0 -> open, -1 -> closing
    float openduration;
    float opentimer;
    float speed;
    float bottom, top;
    bool stayopen;
    sector_t *sector;
} doorthink_t;

typedef struct
{
    thinker_t thinker;
    int state; // 1 -> raising, 0 -> open, -1 -> lowering
    float waitduration;
    float waittimer;
    float speed;
    float bottom, top;
    sector_t *sector;
} platthink_t;

typedef struct
{
    thinker_t thinker;
    float speed;
    float bottom;
    float lastsndtime;
    sector_t* sector;
} floorthink_t;

static void sectorsound(sector_t *sec, sound_e sfxid)
{
    linedef_t *line;
    float x, y;

    line = sec->lines[0];
    x = (line->v1->x + line->v2->x) * 0.5f;
    y = (line->v1->y + line->v2->y) * 0.5f;

    snd_queuepos(sfxid, x, y);
}

bool doorthink(doorthink_t* thinker, float ft, float progtime)
{
    switch(thinker->state)
    {
    case 1:
        thinker->sector->ceilheight += thinker->speed * ft;

        if(thinker->sector->ceilheight >= thinker->top)
        {
            thinker->opentimer = thinker->openduration;
            thinker->sector->ceilheight = thinker->top;
            thinker->state = 0;
        }

        return false;
    case 0:
        thinker->opentimer -= ft;

        if(thinker->opentimer <= 0 && !thinker->stayopen)
        {
            thinker->state = -1;
            sectorsound(thinker->sector, sfx_dorcls);
        }

        return false;
    case -1:

        thinker->sector->ceilheight -= thinker->speed * ft;

        if(level_mobjstuckinsector(thinker->sector))
        {
            thinker->sector->ceilheight += thinker->speed * ft;
            thinker->state = 1;
            sectorsound(thinker->sector, sfx_doropn);
        }
        else if(thinker->sector->ceilheight <= thinker->bottom)
        {
            thinker->sector->ceilheight = thinker->bottom;
            return true;
        }

        return false;
    default:
        assert(0 && "bad doorthink_t state");
        break;
    }

    return false;
}

void doorthinkfree(doorthink_t* thinker)
{
    if(thinker->sector && thinker->sector->thinker == thinker)
        thinker->sector->thinker = NULL;
}

bool platthink(platthink_t* thinker, float ft, float progtime)
{
    switch(thinker->state)
    {
    case -1:
        thinker->sector->floorheight -= thinker->speed * ft;
        if(thinker->sector->floorheight <= thinker->bottom)
        {
            thinker->state = 0;
            thinker->waittimer = thinker->waitduration;
            thinker->sector->floorheight = thinker->bottom;
            sectorsound(thinker->sector, sfx_pstop);
        }

        return false;
    case 0:
        thinker->waittimer -= ft;

        if(thinker->waittimer <= 0)
        {
            thinker->state = 1;
            sectorsound(thinker->sector, sfx_pstart);
        }

        return false;
    case 1:
        thinker->sector->floorheight += thinker->speed * ft;

        if(thinker->sector->floorheight >= thinker->top)
        {
            thinker->sector->floorheight = thinker->top;
            sectorsound(thinker->sector, sfx_pstop);
            return true;
        }

        return false;
    default:
        assert(0 && "bad platthink_t state");
        break;
    }

    return false;
}

void platthinkfree(platthink_t* thinker)
{
    if(thinker->sector && thinker->sector->thinker == thinker)
        thinker->sector->thinker = NULL;
}

bool floorthink(floorthink_t* thinker, float ft, float progtime)
{
    const float soundperiod = 8.0 / 35.0;

    if(progtime - thinker->lastsndtime >= soundperiod)
    {
        sectorsound(thinker->sector, sfx_stnmov);
        thinker->lastsndtime = floorf(progtime / soundperiod) * soundperiod;
    }
    
    thinker->sector->floorheight -= thinker->speed * ft;
    if(thinker->sector->floorheight <= thinker->bottom)
    {
        thinker->sector->floorheight = thinker->bottom;
        sectorsound(thinker->sector, sfx_pstop);
        return true;
    }

    return false;
}

void floorthinkfree(floorthink_t* thinker)
{
    if(thinker->sector && thinker->sector->thinker == thinker)
        thinker->sector->thinker = NULL;
}

bool special_doorsec(object_t* user, sector_t* sec, int special)
{
    doorthink_t *think;
    bool stayopen;

    if(special == 26 || special == 32)
        if(!user->player || (!(user->player->info.flags & PFLAG_BLUCARD) && !(user->player->info.flags & PFLAG_BLUSKUL)))
            return false;
    if(special == 27 || special == 33)
        if(!user->player || (!(user->player->info.flags & PFLAG_YELCARD) && !(user->player->info.flags & PFLAG_YELSKUL)))
            return false;
    if(special == 28 || special == 34)
        if(!user->player || (!(user->player->info.flags & PFLAG_REDCARD) && !(user->player->info.flags & PFLAG_REDSKUL)))
            return false;

    stayopen = false;
    switch (special)
    {
    case 2:
    case 31:
    case 32:
    case 33:
    case 34:
        stayopen = true;
        break;
    default:
        break;
    }

    if(sec->thinker && sec->thinker->func == doorthink)
    {
        think = (doorthink_t*) sec->thinker;

        if(think->stayopen)
            return false;

        think->thinker.func = (thinkfunc_t) doorthink;
        think->thinker.freefunc = (thinkfreefunc_t) doorthinkfree;
        switch(think->state)
        {
        case 1:
            think->state = -1;
            break;
        case 0:
            think->state = -1;
            sectorsound(sec, sfx_dorcls);
            break;
        case -1:
            think->state = 1;
            sectorsound(sec, sfx_doropn);
            break;
        }
        think->openduration = 150.0 / 35.0;
        think->speed = 2.0 * 35.0;
        think->sector = sec;
    }
    else
    {
        if(sec->thinker)
            freethinker(sec->thinker);

        think = calloc(1, sizeof(doorthink_t));
        sec->thinker = think;

        think->thinker.func = (thinkfunc_t) doorthink;
        think->thinker.freefunc = (thinkfreefunc_t) doorthinkfree;
        think->state = 1;
        think->openduration = 150.0 / 35.0;
        think->speed = 2.0 * 35.0;
        think->bottom = sec->ceilheight;
        think->top = level_getlowestneighborceil(sec) - 4;
        think->sector = sec;
        think->stayopen = stayopen;

        sectorsound(sec, sfx_doropn);
        addthinker(think);
    }

    return true;
}

bool special_door(object_t* user, linedef_t* line, int special)
{
    sector_t *sec;

    assert(line->front && line->front->sector && line->back && line->back->sector && "door special on one-sided line");

    sec = line->back->sector;
    return special_doorsec(user, sec, special);
}

bool special_plat(object_t* user, sector_t* sec, int special)
{
    platthink_t *think;

    if(sec->thinker)
        return false;

    think = calloc(1, sizeof(platthink_t));
    sec->thinker = think;

    think->thinker.func = (thinkfunc_t) platthink;
    think->thinker.freefunc = (thinkfreefunc_t) platthinkfree;
    think->state = -1;
    think->waitduration = 105.0 / 35.0;
    think->speed = 4.0 * 35.0;
    think->bottom = level_getlowestneighborfloor(sec);
    think->top = sec->floorheight;
    think->sector = sec;

    sectorsound(sec, sfx_pstart);
    addthinker(think);

    return true;
}

bool special_floorlower(object_t* user, sector_t* sec, int special)
{
    floorthink_t *think;

    if(sec->thinker)
        return false;

    think = calloc(1, sizeof(floorthink_t));
    sec->thinker = think;

    think->thinker.func = (thinkfunc_t) floorthink;
    think->thinker.freefunc = (thinkfreefunc_t) floorthinkfree;
    think->speed = 35.0;
    switch(special)
    {
    case 36:
        think->speed *= 4;
        break;
    default:
        break;
    }
    think->bottom = level_gethighestneighborfloor(sec) + 8;
    think->sector = sec;

    addthinker(think);

    return true;
}
