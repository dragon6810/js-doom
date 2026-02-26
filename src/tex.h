#ifndef _TEX_H
#define _TEX_H

#include <stdint.h>

typedef struct
{
    int x, y, w, h;
    int lumpnum;
} texpatch_t;

typedef struct
{
    char name[9];
    int w, h;
    int npatch;
    texpatch_t *patches;
    int smask;
    int *collumps; // size smask + 1 (wraps at w)
    int *coloffs; // size smask + 1 (wraps at w)
    uint8_t *stitch; // for columns with >1 patch
} texture_t;

extern int ntextures;
extern texture_t *textures;

void tex_dowad(void);
uint8_t* tex_getcolumn(texture_t* tex, int x);
void tex_stitch(texture_t* tex);
texture_t* tex_find(const char* name);

#endif