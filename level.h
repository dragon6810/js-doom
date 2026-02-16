#ifndef _LEVEL_H
#define _LEVEL_H

#include "wad.h"

typedef struct
{
    int x;
    int y;
} vertex_t;

typedef struct
{
    float floorheight;
    float ceilheight;
    lumpinfo_t *floortex;
    lumpinfo_t *ceiltex;
    int light;
    int special;
    int tag;
} sector_t;

extern int nverts;
extern vertex_t *verts;
extern int nsectors;
extern sector_t *sectors;

void level_load(int episode, int map);

#endif