#ifndef _WAD_H
#define _WAD_H

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


#endif
