#include "level.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

int level_nodeside(node_t* node, int x, int y)
{
    float dx, dy, det;

    dx = x - node->x;
    dy = y - node->y;

    det = dx * node->dy - node->dx * dy;
    return det < 0;
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

void level_load(int episode, int map)
{
    char levelname[5];
    lumpinfo_t *lump;

    if(episode < 1 || episode > 4 || map < 1 || map > 9)
    {
        fprintf(stderr, "level_load: bad map E%dM%d\n", episode, map);
        return;
    }

    levelname[0] = 'E';
    levelname[1] = '0' + episode;
    levelname[2] = 'M';
    levelname[3] = '0' + map;

    lump = wad_findlump(levelname, false);
    if(!lump)
    {
        fprintf(stderr, "level_load: wad does not have map E%dM%d\n", episode, map);
        return;
    }

    level_loadverts(lump);
    level_loadsectors(lump);
    level_loadsidedefs(lump);
    level_loadlinedefs(lump);
    level_loadssectors(lump);
    level_loadnodes(lump);
    level_loadsegs(lump);

    printf("loaded %s\n", levelname);
}