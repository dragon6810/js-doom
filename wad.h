#ifndef _WAD_H
#define _WAD_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "info.h"

#define MAX_WAD 16

#define TOTAL_COLORMAP 34
#define LIGHTMAP 32

typedef struct
{
    char name[9];
    int wad;
    int size, loc;

    void *cache;
} lumpinfo_t;

typedef struct __attribute__((packed))
{
    int16_t w;
    int16_t h;
    int16_t	xoffs;
    int16_t	yoffs;
    int32_t postoffs[0]; // length w
    // posts follow
} pic_t;

// total size of post_t is sizeof(post_t) + len + 1
typedef struct __attribute__((packed))
{
    uint8_t ystart; // if 0xFF, terminate
    uint8_t len;
    uint8_t pad;
    uint8_t payload[0]; // length len
    // note additional padding byte after payload
} post_t;

typedef struct __attribute__((packed))
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_t;

typedef struct __attribute((packed))
{
    uint8_t maps[TOTAL_COLORMAP][256];
} colormap_t;

typedef struct
{
    bool rotational;
    int rotlumps[8]; // only use [0] if !rotational
    bool mirror[8];
} sprframe_t;

typedef struct
{
    int nframes;
    sprframe_t *frames; // starts at 'A'
} sprite_t;

extern int nlumps;
extern lumpinfo_t *lumps;
extern int nwads;
extern FILE *wads[MAX_WAD];

extern sprite_t sprites[NUMSPRITES];
extern color_t *palette;
extern colormap_t *colormap;

void wad_load(const char* filename);
lumpinfo_t* wad_findlump(const char* name, bool cache);
void wad_cache(lumpinfo_t* lump);
void wad_decache(lumpinfo_t* lump);
void wad_clearcache(void); // probably should be called at the end of a level
void wad_setpalette(int palnum);

#endif
