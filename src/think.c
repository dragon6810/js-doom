#include "think.h"

#include <stdlib.h>

thinker_t* thinkers = NULL;

void think(float frametime, float progtime)
{
    thinker_t *thinker;

    thinker_t *next;

    for(thinker=thinkers; thinker; thinker=next)
    {
        next = thinker->next;
        if(!thinker->func(thinker, frametime, progtime))
            continue;

        freethinker(thinker);
    }
}

void addthinker(thinker_t* thinker)
{
    thinker->prev = NULL;
    if(thinkers)
        thinkers->prev = thinker;
    thinker->next = thinkers;
    thinkers = thinker;
}

void freethinker(thinker_t* thinker)
{
    thinker_t *prev, *next;
    
    prev = thinker->prev;
    next = thinker->next;

    if(prev)
        prev->next = next;
    if(next)
        next->prev = prev;
    if(thinker == thinkers)
        thinkers = next;

    if(thinker->freefunc)
        thinker->freefunc(thinker);
    
    free(thinker);
}
