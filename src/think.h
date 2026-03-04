#ifndef _THINK_H
#define _THINK_H

#include <stdbool.h>

typedef struct thinker_s thinker_t;

// return true if the thinker should be killed
typedef bool (*thinkfunc_t)(thinker_t* thinker, float ft);

struct thinker_s
{
    thinkfunc_t func;
    thinker_t *prev, *next;
};

extern thinker_t* thinkers;

void think(float frametime);
void addthinker(thinker_t* thinker);

#endif