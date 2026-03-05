#include "stbar.h"

#include "draw.h"
#include "screen.h"
#include "wad.h"

lumpinfo_t *stbarlump;
int sttop;

void stbar_init(void)
{
    stbarlump = wad_findlump("STBAR", true);
    if(!stbarlump)
        fprintf(stderr, "stbar_init: no STBAR lump\n");

    sttop = screenheight - 32.0 * (screenheight / 200.0);
}

void stbar_draw(void)
{
    int x;

    fixed_t texmid;
    fixed_t iscale, scale, s;
    pic_t *pic;
    post_t *post;

    if(!stbarlump)
        return;
    wad_cache(stbarlump);
    pic = stbarlump->cache;

    texmid = -((sttop << FIXEDSHIFT) - (screenheight << (FIXEDSHIFT - 1)));
    iscale = fixeddiv(pic->w << FIXEDSHIFT, screenwidth << FIXEDSHIFT);
    scale = fixeddiv(1 << FIXEDSHIFT, iscale);

    for(x=0, s=0; x<screenwidth; x++, s+=iscale)
    {
        post = (post_t*) ((uint8_t*) pic + pic->postoffs[s >> FIXEDSHIFT]);
        draw_postcolumn(post, colormap->maps[0], x, sttop, screenheight-1, texmid, iscale, scale); 
    }
}