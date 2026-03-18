#include "draw.h"

#include "render.h"
#include "screen.h"

uint8_t transtbls[NUM_MC][256];
screen_t *drawscreen = NULL;

static void draw_remaprange(uint8_t* tbl, int dst, int dstlen, int src, int srclen)
{
    int i;

    fixed_t index;
    fixed_t step; // floating would definetely do here but oh well

    step = fixeddiv(srclen << FIXEDSHIFT, dstlen << FIXEDSHIFT);

    for(i=dst, index=src<<FIXEDSHIFT; i<dst+dstlen; i++, index+=step)
        tbl[i] = index >> FIXEDSHIFT;
}

void draw_init(void)
{
    int i, j;

    for(i=0; i<NUM_MC; i++)
        for(j=0; j<256; j++)
            transtbls[i][j] = j;

    // this is so brittle, i wish these were given in the wad
    draw_remaprange(transtbls[MC_YEL], 112, 16, 160, 8);
    draw_remaprange(transtbls[MC_RED], 112, 16, 176, 16);
    draw_remaprange(transtbls[MC_BLU], 112, 16, 196, 12);
    draw_remaprange(transtbls[MC_BRN], 112, 16, 64, 16);
    draw_remaprange(transtbls[MC_GRY], 112, 16, 96, 16);
    draw_remaprange(transtbls[MC_ROS], 112, 16, 16, 32);
    draw_remaprange(transtbls[MC_ORN], 112, 16, 212, 12);
    draw_remaprange(transtbls[MC_WHT], 112, 16, 80, 32);
    draw_remaprange(transtbls[MC_FRS], 112, 16, 152, 8);
}

void draw_scaledpic(pic_t* pic, uint8_t* trans, uint8_t* colmap, fixed_t x, fixed_t y)
{
    fixed_t s;
    fixed_t iscale, scale, texmid;
    post_t *post;
    int ix, y1, y2;
 
    x -= (fixed_t)pic->xoffs << FIXEDSHIFT;
    y -= (fixed_t)pic->yoffs << FIXEDSHIFT;
 
    scale = fixeddiv(screenheight << FIXEDSHIFT, 200 << FIXEDSHIFT);
    iscale = 0xFFFFFFFFu / (uint32_t) scale + 1;
 
    x = fixedmul(x, scale);
    y = fixedmul(y, scale);

    if(y >> FIXEDSHIFT >= drawscreen->h)
        return;
 
    if(y + (fixed_t)pic->h * scale <= 0)
        return;
    
    texmid = fixedmul((((screenheight >> 1) - drawscreen->y) << FIXEDSHIFT) - y, iscale);
 
    y1 = MAX(y >> FIXEDSHIFT, 0);
    y2 = drawscreen->h - 1;
 
    ix = x >> FIXEDSHIFT;
    if(ix < 0)
    {
        s = fixedmul(-x, iscale);
        ix = 0;
    }
    else
    {
        s = fixedmul(x & 0xFFFF, iscale);
    }
 
    for(; ix < drawscreen->w && (s >> FIXEDSHIFT) < pic->w; ix++, s += iscale)
    {
        post = (post_t*) ((uint8_t*) pic + pic->postoffs[s >> FIXEDSHIFT]);
        if(trans)
            draw_transpostcolumn(post, trans, colmap, ix, y1, y2, texmid, iscale, scale);
        else
            draw_postcolumn(post, colmap, ix, y1, y2, texmid, iscale, scale);
    }
}

void draw_postcolumn(post_t* post, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale)
{
    int y;

    int screeny;
    fixed_t tfrac, startfrac, lenfrac, posttop, halfyfrac;
    fixed_t y1frac, y2frac;
    int dst;

    y1frac = y1 << FIXEDSHIFT;
    y2frac = y2 << FIXEDSHIFT;

    halfyfrac = screenheight << (FIXEDSHIFT - 1);

    while(post->ystart != 0xFF)
    {
        startfrac = (fixed_t) post->ystart << FIXEDSHIFT;
        lenfrac = (fixed_t) post->len << FIXEDSHIFT;

        posttop = fixedmul(startfrac - texmid, scale) + halfyfrac - (drawscreen->y << FIXEDSHIFT);
        if(posttop > y2frac)
            break;

        if(posttop < y1frac)
        {
            tfrac = fixedmul(y1frac - posttop, iscale);
            posttop = y1frac;
            y = posttop >> FIXEDSHIFT;
        }
        else
        {
            y = (posttop + FIXEDUNIT - 1) >> FIXEDSHIFT;
            tfrac = fixedmul((y << FIXEDSHIFT) - posttop, iscale);
        }

        for(dst=y*drawscreen->w+x; ; tfrac+=iscale, y++, dst+=drawscreen->w)
        {
            if(tfrac >= lenfrac || y > y2)
                break;

            drawscreen->pixels[dst] = map[post->payload[tfrac >> FIXEDSHIFT]];
        }

        post = (post_t*) (((uint8_t*) post) + sizeof(post_t) + post->len + 1);
    }
}

void draw_transpostcolumn(post_t* post, uint8_t* trans, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale)
{
    int y;

    fixed_t tfrac, startfrac, lenfrac, posttop, halfyfrac;
    fixed_t y1frac, y2frac;
    int dst;

    y1frac = y1 << FIXEDSHIFT;
    y2frac = y2 << FIXEDSHIFT;

    halfyfrac = screenheight << (FIXEDSHIFT - 1);

    while(post->ystart != 0xFF)
    {
        startfrac = (fixed_t) post->ystart << FIXEDSHIFT;
        lenfrac = (fixed_t) post->len << FIXEDSHIFT;

        posttop = fixedmul(startfrac - texmid, scale) + halfyfrac - (drawscreen->y << FIXEDSHIFT);
        if(posttop > y2frac)
            break;

        if(posttop < y1frac)
        {
            tfrac = fixedmul(y1frac - posttop, iscale);
            posttop = y1frac;
            y = posttop >> FIXEDSHIFT;
        }
        else
        {
            y = (posttop + FIXEDUNIT - 1) >> FIXEDSHIFT;
            tfrac = fixedmul((y << FIXEDSHIFT) - posttop, iscale);
        }

        for(dst=y*drawscreen->w+x; ; tfrac+=iscale, y++, dst+=drawscreen->w)
        {
            if(tfrac >= lenfrac || y > y2)
                break;

            drawscreen->pixels[dst] = map[trans[post->payload[tfrac >> FIXEDSHIFT]]];
        }

        post = (post_t*) (((uint8_t*) post) + sizeof(post_t) + post->len + 1);
    }
}

void draw_texcolumn(uint8_t* col, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale)
{
    int y;

    int dst;
    fixed_t tfrac;

    tfrac = texmid + fixedmul((y1 << FIXEDSHIFT) - (screenheight << (FIXEDSHIFT-1)), iscale);

    dst = (y1 - drawscreen->y) * drawscreen->w + x;
    for(y=y1; y<=y2; y++, tfrac+=iscale, dst+=drawscreen->w)
    {
        drawscreen->pixels[dst] = map[col[(tfrac >> FIXEDSHIFT) & 127]];
    }
}
