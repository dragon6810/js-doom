#ifndef _WAD_H
#define _WAD_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_WAD 16

typedef struct lumpinfo_s
{
    char name[9];
    int wad;
    int size, loc;

    void *cache;
} lumpinfo_t;

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
