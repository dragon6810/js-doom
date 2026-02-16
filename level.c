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
    int16_t x;
    int16_t y;
} mapvertex_t;

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

    printf("%d sectors\n", nsectors);

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

    printf("loaded %s\n", levelname);
}