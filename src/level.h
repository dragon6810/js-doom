#ifndef _LEVEL_H
#define _LEVEL_H

#include "info.h"
#include "doommath.h"
#include "tex.h"
#include "think.h"
#include "wad.h"

#define TICRATE 35

#define THING_EASY           0x0001
#define THING_MEDIUM         0x0002
#define THING_HARD           0x0004
#define THING_AMBUSH         0x0008
#define THING_NOSINGLEPLAYER 0x0010

#define LINEDEF_BLOCKALL      0x0001
#define LINEDEF_BLOCKMONSTERS 0x0002
#define LINEDEF_TWOSIDED      0x0004
#define LINEDEF_UPPERUNPEG    0x0008
#define LINEDEF_LOWERUNPEG    0x0010
#define LINEDEF_SECRET        0x0020
#define LINEDEF_BLOCKSOUND    0x0040
#define LINEDEF_NOMAP         0x0080
#define LINEDEF_MAPPED        0x0100

#define MAX_MOBJ 1024

#define BLOCK_SIZE 128

#define MAX_DMSTART 10

typedef struct object_s object_t;
typedef struct sector_s sector_t;
typedef struct ssector_s ssector_t;
typedef struct sidedef_s sidedef_t;
typedef struct vertex_s vertex_t;
typedef struct block_s block_t;

// these fields are sent from server to client
typedef struct objinfo_s
{
    bool exists;
    float x, y, z;
    angle_t angle;
    statenum_t state;
    mobjtype_t type;
    float xvel, yvel, zvel;
} objinfo_t;

struct object_s
{
    objinfo_t info;

    float timeinstate; // if this * 35 > state's tick duration, go to next state

    int spawnflags;

    ssector_t *ssector;
    block_t *blk;

    thinker_t *thinker;

    object_t *snext, *sprev; // sector list
    object_t *bnext, *bprev; // block list
};

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

struct ssector_s
{
    int nsegs;
    int firstseg;
    
    sector_t *sector;
};

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

    int nlines;
    linedef_t **lines;

    int frameindex;
    object_t *mobjs;

    struct thinker_s *thinker;

    // blockmap bounds
    int bminx, bmaxx, bminy, bmaxy;
};

struct block_s
{
    int nlines;
    linedef_t **lines;

    object_t *mobjs;
};

typedef struct
{
    int xorg, yorg;
    int w, h;
    // y-major, starts in sw
    block_t *blks;
} blockmap_t;

typedef struct
{
    float x, y;
    angle_t angle;
} startloc_t;

extern int level_episode, level_map;

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
extern blockmap_t blockmap;
extern int mobjmax;
extern object_t mobjs[MAX_MOBJ];

extern int numdmstarts;
extern startloc_t dmstarts[MAX_DMSTART];

extern texture_t* levelskytex;

extern float linerangebottom, linerangetop, linerange;

extern float mobjfloorheight, mobjceilheight;

// return true if actual collision
typedef bool (*linelinecol_t)(float x1, float y1, float x2, float y2, linedef_t* line, float t);
typedef bool (*mobjlinecol_t)(float x1, float y1, float x2, float y2, object_t* mobj, float t);

void level_unplacemobj(object_t* mobj);
void level_placemobj(object_t* mobj);
// finds an index to put a new mobj. -1 if edict full
int level_findnewedict(void);
bool level_validobjpos(object_t* mobj, float x, float y);
bool level_traverseline(float x1, float y1, float x2, float y2, bool noearlyexit, linelinecol_t linecol, mobjlinecol_t mobjcol);
void level_mobjheights(object_t* mobj);
int level_nodeside(node_t* node, float x, float y);
int level_lineside(linedef_t* line, float x, float y);
ssector_t* level_getpointssector(float x, float y);
float level_getlowestneighborceil(sector_t* sec);
bool level_mobjstuckinblock(int bx, int by);
void level_setmobjstate(object_t* obj, statenum_t state);
bool level_mobjstuckinsector(sector_t* sector);
void level_addmobjthinker(object_t* obj);
void level_killmobj(object_t* obj);
void level_load(int episode, int map);

#endif