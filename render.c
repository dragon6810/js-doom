#include "render.h"

#include "screen.h"
#include "tex.h"
#include "wad.h"

void render_testtex(void)
{
    const int texid = 1;

    int x, y;

    texture_t *tex;
    color_t *col;

    tex = &textures[texid];

    tex_stitch(tex);

    for(x=0; x<tex->w; x++)
    {
        for(y=0; y<tex->h; y++)
        {
            if (x >= screenwidth || y >= screenheight)
                continue;
            col = &palette[tex->stitch[x * tex->h + y]];
            pixels[y * screenwidth + x] = 0xFF000000 | ((uint32_t) col->r << 16) | ((uint32_t) col->g << 8) | col->b;
        }
    }
}

void render(void)
{
    render_testtex();
}