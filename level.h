#ifndef _LEVEL_H
#define _LEVEL_H

#include "math.h"
#include "tex.h"
#include "wad.h"

#define LINEDEF_BLOCKALL      0x0001
#define LINEDEF_BLOCKMONSTERS 0x0002
#define LINEDEF_TWOSIDED      0x0004
#define LINEDEF_UPPERUNPEG    0x0008
#define LINEDEF_LOWERUNPEG    0x0010
#define LINEDEF_SECRET        0x0020
#define LINEDEF_BLOCKSOUND    0x0040
#define LINEDEF_NOMAP         0x0080
#define LINEDEF_MAPPED        0x0100

typedef struct sector_s sector_t;
typedef struct sidedef_s sidedef_t;
typedef struct vertex_s vertex_t;

typedef struct
{
    vertex_t *v1, *v2;
    int flags;
    int special;
    int tag;
    sidedef_t *front, *back;
} linedef_t;

struct sidedef_s
{
    float xoffs;
    float yoffs;
    texture_t* upper;
    texture_t* lower;
    texture_t* mid;
    sector_t* sector;
};

struct vertex_s
{
    int x;
    int y;
};

typedef struct
{
    vertex_t *v1, *v2;
    angle_t angle;
    linedef_t *line;
    float offset;
    
    sidedef_t *frontside, *backside;
} seg_t;

typedef struct
{
    int nsegs;
    int firstseg;
    
    sector_t *sector;
} ssector_t;

typedef struct
{
    int x, y;
    int dx, dy;
    int bbox[2][4];
    int children[2];
} node_t;

struct sector_s
{
    float floorheight;
    float ceilheight;
    lumpinfo_t *floortex;
    lumpinfo_t *ceiltex;
    int light;
    int special;
    int tag;
};

extern int nverts;
extern vertex_t *verts;
extern int nsectors;
extern sector_t *sectors;
extern int nsidedefs;
extern sidedef_t *sidedefs;
extern int nlinedefs;
extern linedef_t *linedefs;
extern int nssectors;
extern ssector_t *ssectors;
extern int nnodes;
extern node_t *nodes;
extern int nsegs;
extern seg_t *segs;

int level_nodeside(node_t* node, int x, int y);
void level_load(int episode, int map);

#endif