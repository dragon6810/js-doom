#include "tex.h"

#include <stdlib.h>
#include <string.h>

#include "wad.h"

typedef struct __attribute__((packed))
{
    int16_t x;
    int16_t y;
    int16_t patchnum;
    int16_t reserved1;
    int16_t reserved2;
} wadtexpatch_t;

typedef struct __attribute__((packed))
{
    char name[8];
    uint32_t masked;
    int16_t width;
    int16_t height;
    int32_t reserved;
    int16_t patchcount;
    wadtexpatch_t patches[0];
} wadtexture_t;

int ntextures = 0;
texture_t *textures = NULL;

int npnames = 0;
int *pnames = NULL;

void tex_dotexlump(lumpinfo_t* lump)
{
    int i, p;

    int ntexture;
    int offset;
    wadtexture_t *wadtex;
    texture_t *tex;

    if(!lump)
        return;

    wad_cache(lump);

    ntexture = *((int32_t*) lump->cache);
    if(!textures)
        textures = malloc(ntexture * sizeof(texture_t));
    else
        textures = realloc(textures, (ntextures + ntexture) * sizeof(texture_t));
    
    for(i=0; i<ntexture; i++)
    {
        offset = *(((int32_t*) lump->cache) + 1 + i);
        wadtex = lump->cache + offset;
        tex = &textures[ntextures++];

        tex->name[8] = 0;
        memcpy(tex->name, wadtex->name, 8);
        tex->w = wadtex->width;
        tex->h = wadtex->height;
        tex->npatch = wadtex->patchcount;
        tex->patches = malloc(tex->npatch * sizeof(texpatch_t));
        for(p=0; p<tex->npatch; p++)
        {
            tex->patches[p].x = wadtex->patches[p].x;
            tex->patches[p].y = wadtex->patches[p].y;
            tex->patches[p].lumpnum = pnames[wadtex->patches[p].patchnum];
        }
        tex->stitch = NULL;
    }

    wad_decache(lump);
}

void tex_makeptable(void)
{
    int i;

    lumpinfo_t *lump, *patch;
    char name[9];

    lump = wad_findlump("PNAMES", true);
    if(!lump)
        return;

    npnames = *((int32_t*) lump->cache);
    pnames = malloc(npnames * sizeof(*pnames));
    for(i=0; i<npnames; i++)
    {
        pnames[i] = -1;

        memcpy(name, lump->cache + 4 + 8 * i, 8);
        name[8] = 0;

        patch = wad_findlump(name, false);
        if(!patch)
        {
            fprintf(stderr, "tex_makeptable: PNAMES references undefined patch \"%s\"\n", name);
            continue;
        }

        pnames[i] = patch - lumps;
    }

    wad_decache(lump);

    tex_stitch(&textures[0]);
}

void tex_dowad(void)
{
    tex_makeptable();

    tex_dotexlump(wad_findlump("TEXTURE1", false));
    tex_dotexlump(wad_findlump("TEXTURE2", false));

    npnames = 0;
    free(pnames);
}

void tex_stitch(texture_t* tex)
{
    int p, x, y;
    texpatch_t *texpatch;

    lumpinfo_t *patchlump;
    int w, h;
    int columnoffs;
    post_t *post;
    int dstx, dsty;

    if(tex->stitch)
        return;

    tex->stitch = malloc(tex->w * tex->h);
    memset(tex->stitch, 247, tex->w * tex->h);

    for(p=0, texpatch=tex->patches; p<tex->npatch; p++, texpatch++)
    {
        patchlump = &lumps[texpatch->lumpnum];
        wad_cache(patchlump);

        w = *((int16_t*) patchlump->cache);
        h = *((int16_t*) (patchlump->cache + 2));

        for(x=0; x<w; x++)
        {
            columnoffs = *((uint32_t*) (patchlump->cache + 8 + x * 4));
            post = patchlump->cache + columnoffs;

            dstx = texpatch->x + x;
            while(post->ystart != 0xFF)
            {
                for(y=0; y<post->len; y++)
                {
                    dsty = texpatch->y + post->ystart + y;
                    if (dstx >= 0 && dstx < tex->w && dsty >= 0 && dsty < tex->h)
                        tex->stitch[dstx * tex->h + dsty] = post->payload[y];
                }

                post = (post_t*) (((uint8_t*) post) + sizeof(post_t) + post->len + 1);
            }
        }
    }
}

texture_t* tex_find(const char* name)
{
    int i;

    for(i=ntextures-1; i>=0; i--)
        if(!strcasecmp(name, textures[i].name))
            return &textures[i];

    return NULL;
}
