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
    lumpinfo_t *patchlump;
    pic_t *patch;

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

            patchlump = &lumps[tex->patches[p].lumpnum];
            wad_cache(patchlump);
            patch = patchlump->cache;
            tex->patches[p].w = patch->w;
            tex->patches[p].h = patch->h;
            wad_decache(patchlump);
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
}

void tex_dowad(void)
{
    tex_makeptable();

    tex_dotexlump(wad_findlump("TEXTURE1", false));
    tex_dotexlump(wad_findlump("TEXTURE2", false));

    npnames = 0;
    free(pnames);
}

uint8_t* tex_getcolumn(texture_t* tex, int x)
{
    x &= tex->smask;

    if(tex->collumps[x] == -1)
    {
        if(tex->coloffs[x] == -1)
            return NULL;
        return tex->stitch + tex->coloffs[x];
    }
    
    wad_cache(&lumps[tex->collumps[x]]);
    return lumps[tex->collumps[x]].cache + tex->coloffs[x] + 3;
}

// returns new bufwritten
int tex_stitchcolumn(texture_t* tex, int bufwritten, int x)
{
    int i, y;

    int npatch;
    texpatch_t *texpatch;
    lumpinfo_t *patchlump;
    pic_t *patch;
    post_t *post;

    for(i=npatch=0; i<tex->npatch && npatch<2; i++)
    {
        if(tex->patches[i].lumpnum < 0)
            continue;
        if(tex->patches[i].x > x || tex->patches[i].x + tex->patches[i].w <= x)
            continue;
        texpatch = &tex->patches[i];
        npatch++;
    }

    if(!npatch)
    {
        tex->collumps[x] = -1;
        tex->coloffs[x] = -1;
        return bufwritten;
    }

    if(npatch == 1)
    {
        tex->collumps[x] = texpatch->lumpnum;

        patchlump = &lumps[texpatch->lumpnum];
        wad_cache(patchlump);
        patch = patchlump->cache;

        tex->coloffs[x] = patch->postoffs[x - texpatch->x];
        return bufwritten;
    }

    tex->collumps[x] = -1;
    tex->coloffs[x] = bufwritten;

    for(i=0, texpatch=tex->patches; i<tex->npatch; i++, texpatch++)
    {
        if(tex->patches[i].lumpnum < 0)
            continue;
        if(texpatch->x > x || texpatch->x + texpatch->w <= x)
            continue;

        patchlump = &lumps[texpatch->lumpnum];
        wad_cache(patchlump);
        patch = patchlump->cache;
        
        post = (uint8_t*) patch + patch->postoffs[x - texpatch->x];
        while(post->ystart != 0xFF)
        {   
            for(y=0; y<post->len; y++)
            {
                if(y + post->ystart + texpatch->y < 0)
                    continue;
                if(y + post->ystart + texpatch->y >= tex->h)
                    break;
                tex->stitch[bufwritten + y + post->ystart + texpatch->y] = post->payload[y];
            }

            post = (uint8_t*) post + sizeof(post_t) + post->len + 1;
        }
    }

    return bufwritten + tex->h;
}

int tex_countbufsize(texture_t* tex)
{
    int i, x;

    int npatch;
    int size;

    for(x=size=0; x<tex->w; x++)
    {
        for(i=npatch=0; i<tex->npatch&&npatch<2; i++)
        {
            if(tex->patches[i].x > x || tex->patches[i].x + tex->patches[i].w <=x)
                continue;
            npatch++;
        }

        if(npatch >= 2)
            size += tex->h;
    }

    return size;
}

void tex_stitch(texture_t* tex)
{
    int x;

    lumpinfo_t *patchlump;
    post_t *post;
    int paddedw, bufwritten;

    if(tex->stitch)
        return;

    paddedw = 1;
    while(paddedw < tex->w)
        paddedw <<= 1;

    tex->smask = paddedw - 1;
    tex->stitch = malloc(tex_countbufsize(tex));
    tex->collumps = malloc(paddedw * sizeof(int));
    tex->coloffs = malloc(paddedw * sizeof(int));

    for(x=bufwritten=0; x<paddedw; x++)
    {
        if(x >= tex->w)
        {
            tex->collumps[x] = tex->collumps[x % tex->w];
            tex->coloffs[x] = tex->coloffs[x % tex->w];
            continue;
        }

        bufwritten = tex_stitchcolumn(tex, bufwritten, x);
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
