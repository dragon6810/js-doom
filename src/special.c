#include "special.h"

#include <assert.h>
#include <stdlib.h>

#include "think.h"

typedef struct
{
    thinker_t thinker;
    int state; // 1 -> raising, 0 -> open, -1 -> closing
    float opentimer;
    float speed;
    float bottom, top;
    sector_t *sector;
} doorthink_t;

bool doorthink(doorthink_t* thinker, float ft)
{
    switch(thinker->state)
    {
    case 1:
        thinker->sector->ceilheight += thinker->speed * ft;

        if(thinker->sector->ceilheight >= thinker->top)
        {
            thinker->sector->ceilheight = thinker->top;
            thinker->state = 0;
        }
        
        return false;
    case 0:
        thinker->opentimer -= ft;

        if(thinker->opentimer <= 0)
            thinker->state = -1;

        return false;
    case -1:

        thinker->sector->ceilheight -= thinker->speed * ft;

        if(thinker->sector->ceilheight <= thinker->bottom)
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

void special_door(linedef_t* line)
{
    sector_t *sec;
    doorthink_t *think;

    assert(line->front && line->front->sector && line->back && line->back->sector && "door special on one-sided line");

    sec = line->back->sector;

    think = calloc(1, sizeof(doorthink_t));

    think->thinker.func = (thinkfunc_t) doorthink;
    think->state = 1;
    think->opentimer = 150.0 / 35.0;
    think->speed = 2.0 * 35.0;
    think->bottom = sec->ceilheight;
    think->top = level_getlowestneighborceil(sec) - 4;
    think->sector = sec;

    addthinker(think);
}