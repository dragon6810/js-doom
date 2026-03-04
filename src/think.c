#include "think.h"

#include <stdlib.h>

thinker_t* thinkers = NULL;

void think(float frametime)
{
    thinker_t *thinker;

    thinker_t *prev, *next;

    for(thinker=thinkers; thinker; thinker=thinker->next)
    {
        if(!thinker->func(thinker, frametime))
            continue;
        
        prev = thinker->prev;
        next = thinker->next;

        if(prev)
            prev->next = next;
        if(next)
            next->prev = prev;
        if(thinker == thinkers)
            thinkers = next;
        
        free(thinker);
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