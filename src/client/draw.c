#include "draw.h"

#include "screen.h"

void draw_postcolumn(post_t* post, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale)
{
    int y;

    fixed_t tfrac, startfrac, lenfrac, posttop, halfyfrac;
    fixed_t y1frac, y2frac;
    int dst;
    color_t color;

    y1frac = y1 << FIXEDSHIFT;
    y2frac = y2 << FIXEDSHIFT;

    halfyfrac = screenheight << (FIXEDSHIFT - 1);

    while(post->ystart != 0xFF)
    {
        startfrac = (fixed_t) post->ystart << FIXEDSHIFT;
        lenfrac = (fixed_t) post->len << FIXEDSHIFT;

        posttop = fixedmul(startfrac - texmid, scale) + halfyfrac;
        if(posttop > y2frac)
        {
            post = (post_t*) (((uint8_t*) post) + sizeof(post_t) + post->len + 1);
            continue;
        }

        if(posttop < y1frac)
        {
            tfrac = fixedmul(y1frac - posttop, iscale);
            posttop = y1frac;
        }
        else
            tfrac = 0;

        for(y=posttop>>FIXEDSHIFT, dst=y*screenwidth+x; ; tfrac+=iscale, y++, dst+=screenwidth)
        {
            if(tfrac >= lenfrac || y > y2)
                break;

            color = palette[map[post->payload[tfrac >> FIXEDSHIFT]]];
            pixels[dst] = (int) color.r << 16 | (int) color.g << 8 | (int) color.b;
        }
        post = (post_t*) (((uint8_t*) post) + sizeof(post_t) + post->len + 1);
    }
}
