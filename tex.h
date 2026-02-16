#ifndef _TEX_H
#define _TEX_H

#include <stdint.h>

typedef struct
{
    int x, y;
    int lumpnum;
} texpatch_t;

typedef struct
{
    char name[9];
    int w, h;
    int npatch;
    texpatch_t *patches;
    uint8_t *stitch; // is NULL until build
} texture_t;

extern int ntextures;
extern texture_t *textures;

void tex_dowad(void);
void tex_stitch(texture_t* tex);

#endif