#include "level.h"
#include "los.h"

static void explode_mobj(object_t* mobj, int dmg, object_t* exploder, object_t* src)
{
    float dist;
    int attendmg;

    if(mobj == exploder)
        return;

    dist = MAX(fabsf(mobj->info.x-exploder->info.x), fabsf(mobj->info.y-exploder->info.y));
    attendmg = dmg - dist;
    if(attendmg <= 0)
        return;

    if(!lineofsight(mobj, exploder))
        return;
    
    level_damagemobj(mobj, attendmg, exploder, src);
}

static void explode_block(block_t* blk, int dmg, object_t* exploder, object_t* src)
{
    object_t *mobj, *next;

    for(mobj=blk->mobjs; mobj; mobj=next)
    {
        next = mobj->bnext;
        explode_mobj(mobj, dmg, exploder, src);
    }
}

static void explode(int dmg, object_t* exploder, object_t* src)
{
    int bx, by;

    int checkradius;
    int bminx, bminy, bmaxx, bmaxy;

    checkradius = dmg + 32;

    bminx = CLAMP(floorf((exploder->info.x - blockmap.xorg - checkradius) / BLOCK_SIZE), 0, blockmap.w-1);
    bminy = CLAMP(floorf((exploder->info.y - blockmap.yorg - checkradius) / BLOCK_SIZE), 0, blockmap.h-1);
    bmaxx = CLAMP(floorf((exploder->info.x - blockmap.xorg + checkradius) / BLOCK_SIZE), 0, blockmap.w-1);
    bmaxy = CLAMP(floorf((exploder->info.y - blockmap.yorg + checkradius) / BLOCK_SIZE), 0, blockmap.h-1);

    for(bx=bminx; bx<=bmaxx; bx++)
        for(by=bminy; by<=bmaxy; by++)
            explode_block(&blockmap.blks[by * blockmap.w + bx], dmg, exploder, src);
}

void A_Explode()
{
    explode(128, curmobj, curmobj->target ? curmobj->target : curmobj);
}