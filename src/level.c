#include "level.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rand.h"
#include "wad.h"

#define LUMPOFFS_THINGS 1
#define LUMPOFFS_LINEDEFS 2
#define LUMPOFFS_SIDEDEFS 3
#define LUMPOFFS_VERTEXES 4
#define LUMPOFFS_SEGS 5
#define LUMPOFFS_SSECTORS 6
#define LUMPOFFS_NODES 7
#define LUMPOFFS_SECTORS 8
#define LUMPOFFS_REJECT 9
#define LUMPOFFS_BLOCKMAP 10

typedef struct __attribute__((packed))
{
    int16_t x;
    int16_t y;
    int16_t angle;
    int16_t type;
    int16_t flags;
} mapthing_t;

typedef struct __attribute__((packed))
{
    int16_t v1;
    int16_t v2;
    int16_t flags;
    int16_t special;
    int16_t tag;
    int16_t front;
    int16_t back;
} maplinedef_t;

typedef struct __attribute__((packed))
{
    int16_t xoffs;
    int16_t yoffs;
    char upper[8];
    char lower[8];
    char middle[8];
    int16_t sector;
} mapsidedef_t;

typedef struct __attribute__((packed))
{
    int16_t x;
    int16_t y;
} mapvertex_t;

typedef struct __attribute__((packed))
{
    int16_t v1;
    int16_t v2;
    int16_t angle;
    int16_t linedef;
    int16_t direction;
    int16_t offset;
} mapseg_t;

typedef struct __attribute__((packed))
{
    int16_t nsegs;
    int16_t firstseg;
} mapssector_t;

typedef struct __attribute__((packed))
{
    int16_t x;
    int16_t y;
    int16_t dx;
    int16_t dy;
    int16_t bbox[2][4];
    int16_t children[2];
} mapnode_t;

typedef struct __attribute__((packed))
{
    int16_t floorheight;
    int16_t ceilheight;
    char floortex[8];
    char ceiltex[8];
    int16_t light;
    int16_t special;
    int16_t tag;
} mapsector_t;

typedef struct __attribute__((packed))
{
    int16_t xorg;
    int16_t yorg;
    int16_t ncol;
    int16_t nrow;
    int16_t offsets[0];
} mapblockmap_t;

int level_episode = -1, level_map = -1;

int nverts = 0;
vertex_t *verts = NULL;
int nsectors = 0;
sector_t *sectors = NULL;
int nsidedefs = 0;
sidedef_t *sidedefs = NULL;
int nlinedefs = 0;
linedef_t *linedefs = NULL;
int nssectors = 0;
ssector_t *ssectors = NULL;
int nnodes = 0;
node_t *nodes = NULL;
int nsegs = 0;
seg_t *segs = NULL;
int mobjmax;
object_t mobjs[MAX_MOBJ] = {};
blockmap_t blockmap = {};
int numdmstarts = 0;
startloc_t dmstarts[MAX_DMSTART];

texture_t* levelskytex = NULL;
const char *episodeskies[] =
{
    "SKY1", "SKY2", "SKY3", "SKY1", "SKY3"
};

void level_unplacemobj(object_t* mobj)
{
    object_t *cur;

    sector_t *sector;
    block_t *blk;
    object_t *prev, *next;

    if(mobj->ssector)
    {
        sector = mobj->ssector->sector;
        for(cur=sector->mobjs; cur&&cur!=mobj; cur=cur->snext);
        if(cur)
        {
            prev = cur->sprev;
            next = cur->snext;

            if(prev)
                prev->snext = next;
            else
                sector->mobjs = next;
            
            if(next)
                next->sprev = prev;
        }
        mobj->ssector = NULL;
    }

    if(mobj->blk)
    {
        blk = mobj->blk;
        for(cur=blk->mobjs; cur&&cur!=mobj; cur=cur->bnext);
        if(cur)
        {
            prev = cur->bprev;
            next = cur->bnext;

            if(prev)
                prev->bnext = next;
            else
                blk->mobjs = next;

            if(next)
                next->bprev = prev;
        }
        mobj->blk = NULL;
    }
}

void level_placemobj(object_t* mobj)
{
    int bx, by;

    mobj->ssector = level_getpointssector(mobj->info.x, mobj->info.y);
    mobj->sprev = mobj->snext = NULL;

    if(mobj->ssector)
    {
        mobj->snext = mobj->ssector->sector->mobjs;
        mobj->ssector->sector->mobjs = mobj;
        if(mobj->snext)
            mobj->snext->sprev = mobj;
    }

    bx = floorf((mobj->info.x - blockmap.xorg) / BLOCK_SIZE);
    by = floorf((mobj->info.y - blockmap.yorg) / BLOCK_SIZE);
    if(bx >= 0 && bx < blockmap.w && by >= 0 && by < blockmap.h)
    {
        mobj->blk = &blockmap.blks[by * blockmap.w + bx];
        mobj->bprev = NULL;
        mobj->bnext = mobj->blk->mobjs;
        if(mobj->blk->mobjs)
            mobj->blk->mobjs->bprev = mobj;
        mobj->blk->mobjs = mobj;
    }
}

int level_findnewedict(void)
{
    int i;

    for(i=0; i<=mobjmax; i++)
        if(!mobjs[i].info.exists)
            return i;

    if(i >= MAX_MOBJ)
        return -1;

    return ++mobjmax;
}

// returns true if "hit", false if not
typedef bool (*linechecker_t)(object_t*, float x, float y, linedef_t*);

static bool linechecker_mobjmove(object_t* mobj, float x, float y, linedef_t* line)
{
    float floor, ceil;

    if(line->flags & LINEDEF_BLOCKALL)
        return true;

    if(!line->front && !line->back)
        return false;

    if(line->front)
    {
        floor = line->front->sector->floorheight;
        ceil = line->front->sector->ceilheight;
        if(line->back)
        {
            floor = MAX(floor, line->back->sector->floorheight);
            ceil = MIN(ceil, line->back->sector->ceilheight);
        }
    }
    else if(line->back)
    {
        floor = line->back->sector->floorheight;
        ceil = line->back->sector->ceilheight;
        if(line->front)
        {
            floor = MAX(floor, line->front->sector->floorheight);
            ceil = MIN(ceil, line->front->sector->ceilheight);
        }
    }

    if(floor - mobj->info.z > 24)
        return true;

    if(ceil - floor < mobjinfo[mobj->info.type].height)
        return true;

    return false;
}

static bool level_linesquare(linedef_t* line, float radius, float x, float y)
{
    float lx1, ly1, lx2, ly2, dx, dy;
    float minx, miny, maxx, maxy;
    float tmin, tmax, t1, t2, tmp;

    minx = x - radius;
    miny = y - radius;
    maxx = x + radius;
    maxy = y + radius;

    lx1 = line->v1->x;
    ly1 = line->v1->y;
    lx2 = line->v2->x;
    ly2 = line->v2->y;

    tmin = 0;
    tmax = 1;

    dx = lx2 - lx1;
    if(dx == 0)
    {
        if(lx1 <= minx || lx1 >= maxx)
            return false;
    }
    else
    {
        t1 = (minx - lx1) / dx;
        t2 = (maxx - lx1) / dx;
        if(t1 > t2)
        {
            tmp=t1;
            t1=t2;
            t2=tmp;
        }
        tmin = MAX(t1, tmin);
        tmax = MIN(t2, tmax);
        if(tmin >= tmax)
            return false;
    }

    dy = ly2 - ly1;
    if(dy == 0)
    {
        if(ly1 <= miny || ly1 >= maxy)
            return false;
    }
    else
    {
        t1 = (miny - ly1) / dy;
        t2 = (maxy - ly1) / dy;
        if(t1 > t2)
        {
            tmp=t1;
            t1=t2;
            t2=tmp;
        }
        tmin = MAX(t1, tmin);
        tmax = MIN(t2, tmax);
        if(tmin >= tmax)
            return false;
    }

    return true;
}

static bool level_checkobjline(int bx, int by, float x, float y, object_t* obj, linechecker_t checker)
{
    int i;
    block_t *blk;
    linedef_t *line;

    if(bx < 0 || by < 0 || bx >= blockmap.w || by >= blockmap.h)
        return false;

    blk = &blockmap.blks[by * blockmap.w + bx];

    for(i=0; i<blk->nlines; i++)
    {
        line = blk->lines[i];

        if(!level_linesquare(line, mobjinfo[obj->info.type].radius, x, y))
            continue;

        if(checker(obj, x, y, line))
            return true;
    }

    return false;
}

bool level_checksquareobj(int bx, int by, float x, float y, float radius, object_t* ignore)
{
    object_t *mobj;

    block_t *blk;
    float xmin, xmax, ymin, ymax;

    if(bx < 0 || by < 0 || bx >= blockmap.w || by >= blockmap.h)
        return false;

    blk = &blockmap.blks[by * blockmap.w + bx];

    xmin = x - radius;
    xmax = x + radius;
    ymin = y - radius;
    ymax = y + radius;

    for(mobj=blk->mobjs; mobj; mobj=mobj->bnext)
    {
        if(mobj == ignore)
            continue;
        if(!(mobjinfo[mobj->info.type].flags & MF_SOLID))
            continue;

        if(mobj->info.x - mobjinfo[mobj->info.type].radius > xmax)
            continue;
        if(mobj->info.x + mobjinfo[mobj->info.type].radius < xmin)
            continue;
        if(mobj->info.y - mobjinfo[mobj->info.type].radius > ymax)
            continue;
        if(mobj->info.y + mobjinfo[mobj->info.type].radius < ymin)
            continue;
        return true;
    }

    return false;
}

bool level_validobjpos(object_t* mobj, float x, float y)
{
    int bx, by;

    float radius;
    float minx, miny, maxx, maxy;
    int bminx, bminy, bmaxx, bmaxy;

    radius = mobjinfo[mobj->info.type].radius;
    
    minx = x - radius;
    miny = y - radius;
    maxx = x + radius;
    maxy = y + radius;

    bminx = floorf((minx - blockmap.xorg) / BLOCK_SIZE);
    bminy = floorf((miny - blockmap.yorg) / BLOCK_SIZE);
    bmaxx = floorf((maxx - blockmap.xorg) / BLOCK_SIZE);
    bmaxy = floorf((maxy - blockmap.yorg) / BLOCK_SIZE);

    for(bx=bminx; bx<=bmaxx; bx++)
    {
        for(by=bminy; by<=bmaxy; by++)
        {
            if(level_checkobjline(bx, by, x, y, mobj, linechecker_mobjmove))
                return false;
            if(level_checksquareobj(bx, by, x, y, radius, mobj))
                return false;
        }
    }

    return true;
}

bool level_traverseline(float x1, float y1, float x2, float y2, bool noearlyexit, linelinecol_t linecol, mobjlinecol_t mobjcol)
{
    int i;
    object_t *mobj;

    float dx, dy;
    int stepx, stepy;
    float tmaxx, tmaxy, tdx, tdy;
    int bx, by, ex, ey, b;
    linedef_t *line, *bestline;
    object_t *bestmobj;
    float t, bestt, prevt;

    bx = floorf((x1 - blockmap.xorg) / BLOCK_SIZE);
    by = floorf((y1 - blockmap.yorg) / BLOCK_SIZE);
    ex = floorf((x2 - blockmap.xorg) / BLOCK_SIZE);
    ey = floorf((y2 - blockmap.yorg) / BLOCK_SIZE);

    dx = x2 - x1;
    dy = y2 - y1;

    stepx = stepy = 0;
    if(dx > 0)
        stepx = 1;
    else if(dx < 0)
        stepx = -1;
    if(dy > 0)
        stepy = 1;
    else if(dy < 0)
        stepy = -1;

    tmaxx = tmaxy = INFINITY;
    if(stepx > 0)
        tmaxx = ((bx+1)*BLOCK_SIZE - (x1 - blockmap.xorg)) / dx;
    else if(stepx < 0)
        tmaxx = (bx*BLOCK_SIZE - (x1 - blockmap.xorg)) / dx;
    if(stepy > 0)
        tmaxy = ((by+1)*BLOCK_SIZE - (y1 - blockmap.yorg)) / dy;
    else if(stepy < 0)
        tmaxy = (by*BLOCK_SIZE - (y1 - blockmap.yorg)) / dy;

    tdx = BLOCK_SIZE / fabsf(dx);
    tdy = BLOCK_SIZE / fabsf(dy);

    for(;;)
    {
        if(bx >= 0 && by >= 0 && bx < blockmap.w && by < blockmap.h)
        {
            b = by * blockmap.w + bx;
            prevt = -1.0f;
            for(;;)
            {
                bestline = bestmobj = NULL;
                bestt = INFINITY;

                for(i=0; i<blockmap.blks[b].nlines; i++)
                {
                    line = blockmap.blks[b].lines[i];
                    t = segmentsegment(x1, y1, x2, y2, line->v1->x, line->v1->y, line->v2->x, line->v2->y);
                    if(t == INFINITY || t < 0 || t > 1 || t <= prevt)
                        continue;
                    if(t < bestt)
                    {
                        bestline = line;
                        bestt = t;
                    }
                }

                for(mobj=blockmap.blks[b].mobjs; mobj; mobj=mobj->bnext)
                {
                    t = segmentsquare(x1, y1, x2, y2, mobj->info.x, mobj->info.y, mobjinfo[mobj->info.type].radius);

                    if(t == INFINITY || t < 0 || t > 1 || t <= prevt)
                        continue;

                    if(t < bestt)
                    {
                        bestline = NULL;
                        bestmobj = mobj;
                        bestt = t;
                    }
                }

                if(!bestline && !bestmobj)
                    break;
                prevt = bestt;

                if(bestline && linecol && linecol(x1, y1, x2, y2, bestline, bestt) && !noearlyexit)
                    return true;
                if(bestmobj && mobjcol && mobjcol(x1, y1, x2, y2, mobj, bestt) && !noearlyexit)
                    return true;
            }
        }

        if(bx == ex && by == ey)
            break;

        if(tmaxx < tmaxy)
        {
            bx += stepx;
            tmaxx += tdx;
        }
        else if(tmaxy < tmaxx)
        {
            by += stepy;
            tmaxy += tdy;
        }
        else
        {
            bx += stepx;
            by += stepy;
            tmaxx += tdx;
            tmaxy += tdy;
        }
    }

    return false;
}

float mobjfloorheight, mobjceilheight;

void level_mobjheights(object_t* mobj)
{
    int bx, by, l;

    float radius;
    float minx, miny, maxx, maxy;
    int bminx, bminy, bmaxx, bmaxy;
    block_t *blk;
    linedef_t *line;
    ssector_t *ssector;

    radius = mobjinfo[mobj->info.type].radius;
    
    minx = mobj->info.x - radius;
    miny = mobj->info.y - radius;
    maxx = mobj->info.x + radius;
    maxy = mobj->info.y + radius;

    bminx = (minx - blockmap.xorg) / BLOCK_SIZE;
    bminy = (miny - blockmap.yorg) / BLOCK_SIZE;
    bmaxx = (maxx - blockmap.xorg) / BLOCK_SIZE;
    bmaxy = (maxy - blockmap.yorg) / BLOCK_SIZE;

    if(mobj->ssector)
        ssector = mobj->ssector;
    ssector = level_getpointssector(mobj->info.x, mobj->info.y);

    mobjfloorheight = ssector->sector->floorheight;
    mobjceilheight = ssector->sector->ceilheight;
    for(bx=bminx; bx<=bmaxx; bx++)
    {
        for(by=bminy; by<=bmaxy; by++)
        {
            if(bx < 0 || by < 0 || bx >= blockmap.w || by >= blockmap.h)
                continue;
            blk = &blockmap.blks[by * blockmap.w + bx];

            for(l=0; l<blk->nlines; l++)
            {
                line = blk->lines[l];
                if(!level_linesquare(line, radius, mobj->info.x, mobj->info.y))
                    continue;
                
                if(line->front)
                {
                    mobjfloorheight = MAX(mobjfloorheight, line->front->sector->floorheight);
                    mobjceilheight = MIN(mobjceilheight, line->front->sector->ceilheight);
                }
                if(line->back)
                {
                    mobjfloorheight = MAX(mobjfloorheight, line->back->sector->floorheight);
                    mobjceilheight = MIN(mobjceilheight, line->back->sector->ceilheight);
                }
            }
        }
    }
}

int level_nodeside(node_t* node, float x, float y)
{
    float dx, dy, det;

    dx = x - node->x;
    dy = y - node->y;

    det = dx * node->dy - node->dx * dy;
    return det < 0;
}

int level_lineside(linedef_t* line, float x, float y)
{
    float dx, dy, det;

    dx = x - line->v1->x;
    dy = y - line->v1->y;

    det = dx * (line->v2->y-line->v1->y) - (line->v2->x-line->v1->x) * dy;
    return det < 0;
}

ssector_t* level_getpointssector(float x, float y)
{
    int nodenum;

    nodenum = nnodes - 1;
    while(!(nodenum & 0x8000))
        nodenum = nodes[nodenum].children[level_nodeside(&nodes[nodenum], x, y)];

    return &ssectors[nodenum & 0x7FFF];
}

float level_getlowestneighborceil(sector_t* sec)
{
    int i;

    linedef_t *line;

    float ceil;

    ceil = INFINITY;
    for(i=0; i<sec->nlines; i++)
    {
        line = sec->lines[i];
        if(line->front && line->front->sector && line->front->sector != sec)
            ceil = MIN(ceil, line->front->sector->ceilheight);
        if(line->back && line->back->sector && line->back->sector != sec)
            ceil = MIN(ceil, line->back->sector->ceilheight);
    }

    return ceil;
}

bool level_mobjstuckinblock(int bx, int by)
{
    object_t *obj;

    block_t *blk;

    if(bx < 0 || by < 0 || bx >= blockmap.w || by >= blockmap.h)
        return false;

    blk = &blockmap.blks[by * blockmap.w + bx];
    for(obj=blk->mobjs; obj; obj=obj->bnext)
    {
        level_mobjheights(obj);
        if(mobjceilheight - mobjfloorheight < mobjinfo[obj->info.type].height)
            return true;
    }

    return false;
}

bool level_mobjstuckinsector(sector_t* sector)
{
    int bx, by;

    for(bx=sector->bminx; bx<=sector->bmaxx; bx++)
        for(by=sector->bminy; by<=sector->bmaxy; by++)
            if(level_mobjstuckinblock(bx, by))
                return true;

    return false;
}

typedef struct
{
    thinker_t thinker;
    float timeinstate;
    object_t *mobj;
} mobjthink_t;

void level_setmobjstate(object_t* obj, statenum_t state)
{
    obj->info.state = state;
    if(obj->thinker)
        ((mobjthink_t*)obj->thinker)->timeinstate = 0;
}

void level_damagemobj(object_t* obj, int dmg)
{
    int rng;

    obj->info.health -= dmg;
    if(obj->info.health <= 0)
    {
        obj->info.health = 0;
        level_setmobjstate(obj, mobjinfo[obj->info.type].deathstate);
        return;
    }

    if(mobjinfo[obj->info.type].painstate != S_NULL)
    {
        if(prand() <= mobjinfo[obj->info.type].painchance)
        {
            level_setmobjstate(obj, mobjinfo[obj->info.type].painstate);
        }
    }
}

bool level_mobjthink(mobjthink_t* thinker, float ft, float progtime)
{
    thinker->timeinstate += ft;

    while(states[thinker->mobj->info.state].nextstate != S_NULL
    && states[thinker->mobj->info.state].tics
    && thinker->timeinstate > states[thinker->mobj->info.state].tics / 35.0)
    {
        thinker->timeinstate -= states[thinker->mobj->info.state].tics / 35.0;
        thinker->mobj->info.state = states[thinker->mobj->info.state].nextstate;
    }

    return false;
}

void level_mobjthinkfree(mobjthink_t* thinker)
{
    if(thinker->mobj && thinker->mobj->thinker == thinker)
        thinker->mobj->thinker = thinker;
}

void level_addmobjthinker(object_t* obj)
{
    obj->thinker = calloc(1, sizeof(mobjthink_t));
    obj->thinker->func = (thinkfunc_t) level_mobjthink;
    obj->thinker->freefunc = (thinkfreefunc_t) level_mobjthinkfree;
    ((mobjthink_t*)obj->thinker)->timeinstate = 0;
    ((mobjthink_t*)obj->thinker)->mobj = obj;
    addthinker(obj->thinker);
}

void level_killmobj(object_t* obj)
{
    if(obj->thinker)
        freethinker(obj->thinker);
    obj->info.exists = false;
}

void level_loadthings(lumpinfo_t* header)
{
    int i;

    lumpinfo_t *lump;
    int nthings;
    mapthing_t *mapthings;
    object_t *mobj;
    mobjtype_t type;

    lump = header + LUMPOFFS_THINGS;
    wad_cache(lump);

    nthings = lump->size / sizeof(mapthing_t);

    mapthings = lump->cache;

    mobjmax = -1;
    for(i=0; i<nthings; i++)
    {
        // TODO: player starts
        if(mapthings[i].type <= 4)
            continue;

        if(mapthings[i].type == 11)
        {
            if(numdmstarts >= MAX_DMSTART)
                continue;
            dmstarts[numdmstarts].x = mapthings[i].x;
            dmstarts[numdmstarts].y = mapthings[i].y;
            dmstarts[numdmstarts].angle = mapthings[i].angle;
            numdmstarts++;
            continue;
        }

        for(type=0; type<NUMMOBJTYPES; type++)
            if(mobjinfo[type].doomednum == mapthings[i].type)
                break;

        if(type >= NUMMOBJTYPES)
        {
            fprintf(stderr, "level_loadthings: unrecognized doomed id %d\n", mapthings[i].type);
            continue;
        }

        if(mobjinfo[type].flags & MF_NOTDMATCH || mobjinfo[type].flags & MF_COUNTKILL)
            continue;

        if(mobjmax >= MAX_MOBJ-1)
        {
            fprintf(stderr, "level_loadthings: level has too many things\n");
            break;
        }

        mobj = &mobjs[++mobjmax];
        mobj->info.exists = true;
        mobj->info.type = type;
        mobj->info.x = mapthings[i].x;
        mobj->info.y = mapthings[i].y;
        mobj->info.angle = mapthings[i].angle * ANG1;
        mobj->spawnflags = mapthings[i].flags;

        mobj->info.state = mobjinfo[mobj->info.type].spawnstate;

        level_placemobj(mobj);
        mobj->info.z = mobj->ssector->sector->floorheight;

        level_addmobjthinker(mobj);
    }

    wad_decache(lump);
}

void level_linksectors(void)
{
    int ss, l, s;

    ssector_t *ssector;
    linedef_t *line;
    sector_t *sector;

    for(ss=0, ssector=ssectors; ss<nssectors; ss++, ssector++)
    {
        if(!ssector->nsegs)
            continue;
        ssector->sector = segs[ssector->firstseg].frontside->sector;
    }

    for(l=0, line=linedefs; l<nlinedefs; l++, line++)
    {
        if(!line->front || !line->front->sector)
            continue;
        line->front->sector->nlines++;
        if(line->back && line->back->sector && (line->back->sector != line->front->sector))
            line->back->sector->nlines++;
    }

    for(s=0, sector=sectors; s<nsectors; s++, sector++)
    {
        sector->lines = calloc(sector->nlines, sizeof(linedef_t*));
        sector->nlines = 0;
    }

    for(l=0, line=linedefs; l<nlinedefs; l++, line++)
    {
        if(!line->front || !line->front->sector)
            continue;
        line->front->sector->lines[line->front->sector->nlines++] = line;
        if(line->back && line->back->sector && (line->back->sector != line->front->sector))
            line->back->sector->lines[line->back->sector->nlines++] = line;
    }

    for(s=0, sector=sectors; s<nsectors; s++, sector++)
    {
        sector->bminx = blockmap.w;
        sector->bminy = blockmap.h;
        sector->bmaxx = -1;
        sector->bmaxy = -1;
        for(l=0; l<sector->nlines; l++)
        {
            line = sector->lines[l];
            sector->bminx = MIN(sector->bminx, floorf((line->v1->x - blockmap.xorg) / BLOCK_SIZE));
            sector->bminx = MIN(sector->bminx, floorf((line->v2->x - blockmap.xorg) / BLOCK_SIZE));
            sector->bmaxx = MAX(sector->bmaxx, floorf((line->v1->x - blockmap.xorg) / BLOCK_SIZE));
            sector->bmaxx = MAX(sector->bmaxx, floorf((line->v2->x - blockmap.xorg) / BLOCK_SIZE));
            sector->bminy = MIN(sector->bminy, floorf((line->v1->y - blockmap.yorg) / BLOCK_SIZE));
            sector->bminy = MIN(sector->bminy, floorf((line->v2->y - blockmap.yorg) / BLOCK_SIZE));
            sector->bmaxy = MAX(sector->bmaxy, floorf((line->v1->y - blockmap.yorg) / BLOCK_SIZE));
            sector->bmaxy = MAX(sector->bmaxy, floorf((line->v2->y - blockmap.yorg) / BLOCK_SIZE));
        }

        sector->bminx = CLAMP(sector->bminx, 0, blockmap.w - 1);
        sector->bmaxx = CLAMP(sector->bmaxx, 0, blockmap.w - 1);
        sector->bminy = CLAMP(sector->bminy, 0, blockmap.h - 1);
        sector->bmaxy = CLAMP(sector->bmaxy, 0, blockmap.h - 1);
    }
}

void level_loadverts(lumpinfo_t* header)
{
    int i;

    lumpinfo_t *lump;
    mapvertex_t *mapverts;

    lump = header + LUMPOFFS_VERTEXES;
    wad_cache(lump);

    nverts = lump->size / sizeof(mapvertex_t);
    mapverts = lump->cache;
    verts = malloc(nverts * sizeof(vertex_t));

    for(i=0; i<nverts; i++)
    {
        verts[i].x = mapverts[i].x;
        verts[i].y = mapverts[i].y;
    }

    wad_decache(lump);
}

void level_loadsectors(lumpinfo_t* header)
{
    int i;

    lumpinfo_t *lump;
    mapsector_t *mapsectors;
    char name[9];

    lump = header + LUMPOFFS_SECTORS;
    wad_cache(lump);

    nsectors = lump->size / sizeof(mapsector_t);
    mapsectors = lump->cache;
    sectors = malloc(nsectors * sizeof(sector_t));

    name[8] = 0;
    for(i=0; i<nsectors; i++)
    {
        sectors[i].floorheight = mapsectors[i].floorheight;
        sectors[i].ceilheight = mapsectors[i].ceilheight;
        memcpy(name, mapsectors[i].floortex, 8);
        sectors[i].floortex = wad_findlump(name, false);
        memcpy(name, mapsectors[i].ceiltex, 8);
        sectors[i].ceiltex = wad_findlump(name, false);
        sectors[i].light = mapsectors[i].light;
        sectors[i].special = mapsectors[i].special;
        sectors[i].tag = mapsectors[i].tag;
        sectors[i].frameindex = -1;
        sectors[i].mobjs = NULL;
        sectors[i].nlines = 0;
        sectors[i].lines = NULL;
        sectors[i].thinker = NULL;
        sectors[i].bminx = sectors[i].bmaxx = sectors[i].bminy = sectors[i].bmaxy = -1;
    }

    wad_decache(lump);
}

void level_loadsidedefs(lumpinfo_t* header)
{
    int i;

    lumpinfo_t *lump;
    mapsidedef_t *mapsides;
    char name[9];

    lump = header + LUMPOFFS_SIDEDEFS;
    wad_cache(lump);

    nsidedefs = lump->size / sizeof(mapsidedef_t);
    mapsides = lump->cache;
    sidedefs = malloc(nsidedefs * sizeof(sidedef_t));

    name[8] = 0;
    for(i=0; i<nsidedefs; i++)
    {
        sidedefs[i].xoffs = mapsides[i].xoffs;
        sidedefs[i].yoffs = mapsides[i].yoffs;
        memcpy(name, mapsides[i].upper, 8);
        sidedefs[i].upper = tex_find(name);
        memcpy(name, mapsides[i].lower, 8);
        sidedefs[i].lower = tex_find(name);
        memcpy(name, mapsides[i].middle, 8);
        sidedefs[i].mid = tex_find(name);
        sidedefs[i].sector = &sectors[mapsides[i].sector];
    }

    wad_decache(lump);
}

void level_loadlinedefs(lumpinfo_t* header)
{
    int i;

    lumpinfo_t *lump;
    maplinedef_t *maplines;

    lump = header + LUMPOFFS_LINEDEFS;
    wad_cache(lump);

    nlinedefs = lump->size / sizeof(maplinedef_t);
    maplines = lump->cache;
    linedefs = malloc(nlinedefs * sizeof(linedef_t));

    for(i=0; i<nlinedefs; i++)
    {
        linedefs[i].v1 = &verts[maplines[i].v1];
        linedefs[i].v2 = &verts[maplines[i].v2];
        linedefs[i].flags = maplines[i].flags;
        linedefs[i].special = maplines[i].special;
        linedefs[i].tag = maplines[i].tag;
        if(maplines[i].front >= 0)
            linedefs[i].front = &sidedefs[maplines[i].front];
        else
            linedefs[i].front = NULL;
        if(maplines[i].back >= 0)
            linedefs[i].back = &sidedefs[maplines[i].back];
        else
            linedefs[i].back = NULL;
    }

    wad_decache(lump);
}

void level_loadssectors(lumpinfo_t* header)
{
    int i;

    lumpinfo_t *lump;
    mapssector_t *mapssectors;

    lump = header + LUMPOFFS_SSECTORS;
    wad_cache(lump);

    nssectors = lump->size / sizeof(mapssector_t);
    mapssectors = lump->cache;
    ssectors = malloc(nssectors * sizeof(ssector_t));

    for(i=0; i<nssectors; i++)
    {
        ssectors[i].nsegs = mapssectors[i].nsegs;
        ssectors[i].firstseg = mapssectors[i].firstseg;
        ssectors[i].sector = NULL;
    }

    wad_decache(lump);
}

void level_loadnodes(lumpinfo_t* header)
{
    int i, j, k;

    lumpinfo_t *lump;
    mapnode_t *mapnodes;

    lump = header + LUMPOFFS_NODES;
    wad_cache(lump);

    nnodes = lump->size / sizeof(mapnode_t);
    mapnodes = lump->cache;
    nodes = malloc(nnodes * sizeof(node_t));

    for(i=0; i<nnodes; i++)
    {
        nodes[i].x = mapnodes[i].x;
        nodes[i].y = mapnodes[i].y;
        nodes[i].dx = mapnodes[i].dx;
        nodes[i].dy = mapnodes[i].dy;
        for(j=0; j<2; j++)
        {
            for(k=0; k<4; k++)
                nodes[i].bbox[j][k] = mapnodes[i].bbox[j][k];
            nodes[i].children[j] = mapnodes[i].children[j];
        }
    }

    wad_decache(lump);
}

void level_loadsegs(lumpinfo_t* header)
{
    int i;

    lumpinfo_t *lump;
    mapseg_t *mapsegs;

    lump = header + LUMPOFFS_SEGS;
    wad_cache(lump);

    nsegs = lump->size / sizeof(mapseg_t);
    mapsegs = lump->cache;
    segs = malloc(nsegs * sizeof(seg_t));

    for(i=0; i<nsegs; i++)
    {
        segs[i].v1 = &verts[mapsegs[i].v1];
        segs[i].v2 = &verts[mapsegs[i].v2];
        segs[i].angle = mapsegs[i].angle << 16;
        segs[i].line = &linedefs[mapsegs[i].linedef];
        segs[i].offset = mapsegs[i].offset;
        if(!mapsegs[i].direction)
        {
            segs[i].frontside = segs[i].line->front;
            segs[i].backside = segs[i].line->back;
        }
        else
        {
            segs[i].frontside = segs[i].line->back;
            segs[i].backside = segs[i].line->front;
        }
    }

    wad_decache(lump);
}

void level_loadblockmap(lumpinfo_t* header)
{
    int i, j;

    lumpinfo_t *lump;
    mapblockmap_t *mapblks;
    uint16_t *lines;

    lump = header + LUMPOFFS_BLOCKMAP;
    wad_cache(lump);

    mapblks = lump->cache;

    blockmap.xorg = mapblks->xorg;
    blockmap.yorg = mapblks->yorg;
    blockmap.w = mapblks->ncol;
    blockmap.h = mapblks->nrow;
    blockmap.blks = malloc(blockmap.w * blockmap.h * sizeof(block_t));
    memset(blockmap.blks, 0, blockmap.w * blockmap.h * sizeof(block_t));

    for(i=0; i<blockmap.w*blockmap.h; i++)
    {
        lines = (uint16_t*) mapblks + mapblks->offsets[i];
        for(j=0; lines[j]!=0xFFFF; j++);
        
        blockmap.blks[i].nlines = j;
        blockmap.blks[i].lines = malloc(j * sizeof(linedef_t*));
        for(j=0; j<blockmap.blks[i].nlines; j++)
            blockmap.blks[i].lines[j] = &linedefs[lines[j]];
    }

    wad_decache(lump);
}

void level_load(int e, int m)
{
    char levelname[5];
    lumpinfo_t *lump;

    level_episode = e;
    level_map = m;

    if(level_episode < 1 || level_episode > 4 || level_map < 1 || level_map > 9)
    {
        fprintf(stderr, "level_load: bad map E%dM%d\n", level_episode, level_map);
        return;
    }

    levelname[0] = 'E';
    levelname[1] = '0' + level_episode;
    levelname[2] = 'M';
    levelname[3] = '0' + level_map;
    levelname[4] = 0;

    lump = wad_findlump(levelname, false);
    if(!lump)
    {
        fprintf(stderr, "level_load: wad does not have map E%dM%d\n", level_episode, level_map);
        return;
    }

    level_loadverts(lump);
    level_loadsectors(lump);
    level_loadsidedefs(lump);
    level_loadlinedefs(lump);
    level_loadssectors(lump);
    level_loadnodes(lump);
    level_loadsegs(lump);
    level_loadblockmap(lump);

    level_linksectors();
    level_loadthings(lump);

    assert(numdmstarts);

    levelskytex = tex_find(episodeskies[level_episode - 1]);

    printf("loaded %s\n", levelname);
}