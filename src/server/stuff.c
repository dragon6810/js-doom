#include "level.h"

void A_Fall(void)
{
    if(!curmobj)
        return;

    curmobj->info.flags &= ~MF_SOLID;
    curmobj->info.flags &= ~MF_SHOOTABLE;
    curmobj->info.height /= 4;
}