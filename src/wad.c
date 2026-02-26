#include "wad.h"

#include <stdlib.h>
#include <string.h>

#include "tex.h"

int nlumps = 0;
lumpinfo_t *lumps = NULL;
int nwads = 0;
FILE *wads[MAX_WAD];

sprite_t sprites[NUMSPRITES];
color_t *palette = NULL;
colormap_t *colormap = NULL;
lumpinfo_t *skylump = NULL;

int sprstart, sprend;

void wad_loadcolormap(void)
{
    lumpinfo_t *lump;

    lump = wad_findlump("COLORMAP", true);
    if(!lump)
    {
        colormap = NULL;
        return;   
    }

    colormap = lump->cache;
}

sprframe_t tempframes[256 - 'A'];

void wad_findsprite(sprite_t* spr, const char* name)
{
    int i, l;
    lumpinfo_t *lump;

    int maxframe, frame, rot;
    sprframe_t *pframe;

    memset(tempframes, 0, sizeof(tempframes));

    maxframe = -1;
    for(l=sprstart+1; l<sprend; l++)
    {
        lump = &lumps[l];

        if(strncasecmp(name, lump->name, 4))
            continue;
        
        frame = lump->name[4] - 'A';
        if(frame > maxframe)
            maxframe = frame;
        rot = lump->name[5] - '0';

        if(frame >= 0 && rot >= 0 && rot <= 8)
        {
            pframe = &tempframes[frame];
            pframe->rotational = rot != 0;
            if(rot == 0)
            {
                pframe->rotational = false;
                pframe->rotlumps[0] = l;
                pframe->mirror[0] = false;
            }
            else
            {
                pframe->rotational = true;
                pframe->rotlumps[rot - 1] = l;
                pframe->mirror[rot - 1] = false;
            }
        }

        if(!lump->name[6])
            continue;
        
        frame = lump->name[6] - 'A';
        if(frame > maxframe)
            maxframe = frame;
        rot = lump->name[7] - '0';

        if(frame >= 0 && rot >= 0 && rot <= 8)
        {
            pframe = &tempframes[frame];
            pframe->rotational = rot != 0;
            if(rot == 0)
            {
                pframe->rotational = false;
                pframe->rotlumps[0] = l;
                pframe->mirror[0] = true;
            }
            else
            {
                pframe->rotational = true;
                pframe->rotlumps[rot - 1] = l;
                pframe->mirror[rot - 1] = true;
            }
        }
    }

    if(maxframe < 0)
    {
        spr->nframes = 0;
        spr->frames = NULL;
        return;
    }

    spr->nframes = maxframe + 1;
    spr->frames = malloc(spr->nframes * sizeof(sprframe_t));
    memcpy(spr->frames, tempframes, spr->nframes * sizeof(sprframe_t));
}

void wad_findsprites(void)
{
    int i;

    for(i=0; i<NUMSPRITES; i++)
        wad_findsprite(&sprites[i], sprnames[i]);
}

void wad_loadlumpinfos(int32_t nlump, int32_t loc)
{
    int i;

    FILE *ptr;
    lumpinfo_t *plump;
    int32_t filepos, size;

    ptr = wads[nwads-1];
    for(i=0; i<nlump; i++)
    {
        plump = &lumps[nlumps++];
        fseek(ptr, loc + i * 16, SEEK_SET);

        fread(&filepos, sizeof(int32_t), 1, ptr);
        fread(&size, sizeof(int32_t), 1, ptr);
        fread(plump->name, 1, 8, ptr);

        plump->loc = filepos;
        plump->size = size;
        plump->name[8] = 0;
        plump->wad = nwads-1;
        plump->cache = NULL;

        if(!strcmp(plump->name, "S_START") || !strcmp(plump->name, "SS_START"))
            sprstart = i;
        else if(!strcmp(plump->name, "S_END") || !strcmp(plump->name, "SS_END"))
            sprend = i;
    }
}

void wad_load(const char* filename)
{
    char magic[5];
    int32_t nlump, lumpinfoloc;

    sprstart = sprend = -1;

    if(nwads >= MAX_WAD)
    {
        fprintf(stderr, "wad_load: %d max wads, skipped \"%s\"\n", MAX_WAD, filename);
        return;
    }

    FILE *ptr = fopen(filename, "rb");
    if(!ptr)
    {
        fprintf(stderr, "wad_load: could not open file \"%s\"\n", filename);
        return;
    }

    magic[4] = 0;
    fread(magic, 1, 4, ptr);
    if(strcmp(magic, "IWAD") && strcmp(magic, "PWAD"))
    {
        fclose(ptr);
        fprintf(stderr, "wad_load: file \"%s\" is not a valid wad file\n", filename);
        return;
    }

    fread(&nlump, sizeof(int32_t), 1, ptr);
    fread(&lumpinfoloc, sizeof(int32_t), 1, ptr);

    if(!lumps)
        lumps = malloc(nlump * sizeof(lumpinfo_t));
    else
        lumps = realloc(lumps, (nlumps + nlump) * sizeof(lumpinfo_t));

    wads[nwads++] = ptr;
    wad_loadlumpinfos(nlump, lumpinfoloc);

    tex_dowad();
    wad_findsprites();
    wad_loadcolormap();

    skylump = wad_findlump("F_SKY1", false);
}

lumpinfo_t* wad_findlump(const char* name, bool cache)
{
    int i;

    for(i=nlumps-1; i>=0; i--)
    {
        if(strcasecmp(lumps[i].name, name))
            continue;
        
        if(cache)
            wad_cache(&lumps[i]);
        return &lumps[i];
    }

    return NULL;
}

void wad_cache(lumpinfo_t* lump)
{
    FILE *ptr;

    if(lump->cache)
        return;

    ptr = wads[lump->wad];
    fseek(ptr, lump->loc, SEEK_SET);

    lump->cache = malloc(lump->size);
    fread(lump->cache, 1, lump->size, ptr);
}

void wad_decache(lumpinfo_t* lump)
{
    if(!lump->cache)
        return;
    free(lump->cache);
    lump->cache = NULL;
}

void wad_clearcache(void)
{
    int i;

    for(i=0; i<nlumps; i++)
    {
        if(lumps[i].cache)
            continue;
        free(lumps[i].cache);
        lumps[i].cache = NULL;
    }
}

void wad_setpalette(int palnum)
{
    lumpinfo_t *lump;

    lump = wad_findlump("PLAYPAL", true);
    if(!lump)
    {
        fprintf(stderr, "wad_setpalette: no PLAYPAL lump\n");
        return;
    }

    if(palnum < 0 || palnum > lump->size / 768)
    {
        fprintf(stderr, "wad_setpalette: palnum out of bounds\n");
        return;
    }

    palette = lump->cache + palnum * 768;
}
