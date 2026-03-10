#include "special.h"

#include <assert.h>
#include <stdlib.h>

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
    sector_t *sector;
} doorthink_t;

static void doorsound(sector_t *sec, int sfxid)
{
    linedef_t *l = sec->lines[0];
    float x = (l->v1->x + l->v2->x) * 0.5f;
    float y = (l->v1->y + l->v2->y) * 0.5f;
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

        if(thinker->opentimer <= 0)
        {
            thinker->state = -1;
            doorsound(thinker->sector, SFX_DORCLS);
        }

        return false;
    case -1:

        thinker->sector->ceilheight -= thinker->speed * ft;

        if(level_mobjstuckinsector(thinker->sector))
        {
            thinker->sector->ceilheight += thinker->speed * ft;
            thinker->state = 1;
            doorsound(thinker->sector, SFX_DOROPN);
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

void special_door(linedef_t* line)
{
    sector_t *sec;
    doorthink_t *think;

    assert(line->front && line->front->sector && line->back && line->back->sector && "door special on one-sided line");

    sec = line->back->sector;

    if(sec->thinker && sec->thinker->func == doorthink)
    {
        think = (doorthink_t*) sec->thinker;

        think->thinker.func = (thinkfunc_t) doorthink;
        think->thinker.freefunc = (thinkfreefunc_t) doorthinkfree;
        switch(think->state)
        {
        case 1:
            think->state = -1;
            doorsound(sec, SFX_DORCLS);
            break;
        case 0:
            think->state = -1;
            doorsound(sec, SFX_DORCLS);
            break;
        case -1:
            think->state = 1;
            doorsound(sec, SFX_DOROPN);
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

        doorsound(sec, SFX_DOROPN);
        addthinker(think);
    }
}
