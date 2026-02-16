#ifndef _WAD_H
#define _WAD_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_WAD 16

typedef struct
{
    char name[9];
    int wad;
    int size, loc;

    void *cache;
} lumpinfo_t;

// total size of post_t is sizeof(post_t) + len + 1
typedef struct
{
    uint8_t ystart; // if 0xFF, terminate
    uint8_t len;
    uint8_t pad;
    uint8_t payload[0]; // length len
    // note additional padding byte after payload
} post_t;

extern int nlumps;
extern lumpinfo_t *lumps;
extern int nwads;
extern FILE *wads[MAX_WAD];

void wad_load(const char* filename);
lumpinfo_t* wad_findlump(const char* name, bool cache);
void wad_cache(lumpinfo_t* lump);
void wad_decache(lumpinfo_t* lump);
void wad_clearcache(void); // probably should be called at the end of a level

#endif
