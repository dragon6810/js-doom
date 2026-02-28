#include "render.h"

#include "client.h"
#include "level.h"
#include "screen.h"
#include "tex.h"
#include "wad.h"

#include <stdlib.h>
#include <string.h>

#define MAX_CLIPSPAN 192
#define MAX_VISPLANE 256
#define MAX_DRAWSEG  512
#define MAX_VISTHING 256
#define FLAT_RES 64

#define LIGHTLEVELS 16
#define LIGHTSHIFT 4 // 0-255 to 0-15
#define SCALEBANDS 48
#define ZBANDS 128

#define SKYW 256
#define SKYH 128

float viewx = 0, viewy = 0, viewz = 0;
angle_t viewangle = 0;

int frameindex = 0;

uint8_t *scalemap[LIGHTLEVELS][SCALEBANDS];
uint8_t *zmap[LIGHTLEVELS][ZBANDS];

typedef struct
{
    int16_t x1, x2;
} clipspan_t;

typedef struct visthing_s
{
    uint8_t *colormap;
    bool mirror;
    pic_t *patch;
    fixed_t scale;
    int x;
    int y;

    struct visthing_s *next, *prev;
} visthing_t;

typedef struct
{
    seg_t *seg;
    int16_t *bottom, *top; // both NULL, solid wall. one null, top or bottom wall
    int16_t *maskeds;
    float scale1, scale2;
    float scalestep;
    int x1, x2;
} drawseg_t;

typedef struct
{
    int16_t x1, x2;
    int16_t *tops;
    int16_t *bottoms;
    float z; // relative to viewz
    lumpinfo_t *flat;
    int baselight;
} visplane_t;

float projectconst = 0;
fixed_t projectfrac = 0;
float halfx = 0, halfy = 0;

int nclipspans = 0;
clipspan_t clipspans[MAX_CLIPSPAN];
int16_t *topclips = NULL;
int16_t *bottomclips = NULL;
int16_t *spanstarts = NULL;

int nvisplanes = 0;
visplane_t visplanes[MAX_VISPLANE];

int clipbuffsize = 0;
int16_t *clipbuff = NULL, *clipend = NULL;

int ndrawsegs = 0;
drawseg_t drawsegs[MAX_DRAWSEG];

int nvisthings = 0;
visthing_t visthings[MAX_VISTHING];
visthing_t *visthinghead = NULL, *visthingtail = NULL;

angle_t unclippeda1;
int16_t *visthingtop;
int16_t *visthingbottom;

const int npastelcolors = 10;
const int pastelcolors[] = { 16, 51, 83, 115, 161, 171, 194, 211, 226, 250 };

void render_init(void)
{
    int i, j;

    int baselevel, level;
    float scale;

    wad_setpalette(0);
    wad_loadcolormap();

    if(topclips)
        free(topclips);
    if(bottomclips)
        free(bottomclips);

    topclips = malloc(screenwidth * sizeof(int16_t));
    bottomclips = malloc(screenwidth * sizeof(int16_t));

    for(i=0; i<MAX_VISPLANE; i++)
    {
        if(visplanes[i].tops)
            free(visplanes[i].tops);
        if(visplanes[i].bottoms)
            free(visplanes[i].bottoms);

        visplanes[i].tops = malloc(screenwidth * sizeof(int16_t));
        visplanes[i].bottoms = malloc(screenwidth * sizeof(int16_t));
    }

    if(spanstarts)
        free(spanstarts);

    spanstarts = malloc(screenheight * sizeof(int16_t));

    if(clipbuff)
        free(clipbuff);

    clipbuffsize = 64 * screenwidth;
    clipbuff = malloc(clipbuffsize * sizeof(int16_t));

    if(visthingtop)
        free(visthingtop);
    if(visthingbottom)
        free(visthingbottom);

    visthingtop = malloc(screenwidth * sizeof(int16_t));
    visthingbottom = malloc(screenwidth * sizeof(int16_t));

    projectconst = (float) screenwidth / HPLANE;
    projectfrac = FLOATTOFIXED(projectconst);
    halfx = (float) screenwidth / 2.0;
    halfy = (float) screenheight / 2.0;

    for(i=0; i<LIGHTLEVELS; i++)
    {
        baselevel = ((LIGHTLEVELS - 1 - i) * 2) * LIGHTMAP / LIGHTLEVELS;
        for(j=0; j<SCALEBANDS; j++)
        {
            level = baselevel - j / 2;
            level = CLAMP(level, 0, LIGHTMAP - 1);
            scalemap[i][j] = colormap->maps[level];
        }

        for(j=0; j<ZBANDS; j++)
        {
            scale = halfx / (float) (j + 1);
            level = baselevel - scale / 2;
            level = CLAMP(level, 0, LIGHTMAP - 1);
            zmap[i][j] = colormap->maps[level];
        }
    }
}

int render_angletox(angle_t angle)
{
    const float halfplane = HPLANE / 2;

    float alpha;
    int x;

    // screen left -> (1, -1) <- screen right
    alpha = ANGTAN(angle) / halfplane;

    x = (-alpha / 2.0 + 0.5) * screenwidth;
    if(x < 0)
        x = 0;
    if(x > screenwidth)
        x = screenwidth;
    
    return x;
}

angle_t render_xtoangle(int x)
{
    const float halfplane = HPLANE / 2;

    float f, alpha, tangent;

    f = (float) x + 0.5;
    alpha = 1.0 - (2.0 * f) / (float) screenwidth;
    tangent = alpha * halfplane;
    
    return ANGATAN(tangent);
}

angle_t render_ytoangle(int y)
{
    const float halfplane = VPLANE / 2.0f;

    float alpha = (halfy - y) / halfy;
    float tangent = alpha * halfplane;
    
    return ANGATAN(tangent);
}

void render_postcolumn(post_t* post, uint8_t* map, int x, int y1, int y2, fixed_t texmid, fixed_t iscale, fixed_t scale)
{
    int y;

    fixed_t tfrac, startfrac, lenfrac, posttop, halfyfrac;
    fixed_t y1frac, y2frac;
    int dst;
    color_t color;

    y1frac = y1 << FIXEDSHIFT;
    y2frac = y2 << FIXEDSHIFT;

    halfyfrac = FLOATTOFIXED(halfy);

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

void render_maskedseg(drawseg_t* seg, int x1, int x2, fixed_t maxscale)
{
    int x;

    fixed_t halfyfrac;
    fixed_t scalefrac, startx, scalestepfrac, top, topstep, height, hstep, iscale;
    fixed_t ttop, portaltop, portalbottom;
    int pxtop, pxbottom;
    int baselight;
    int lightindex;
    fixed_t texmid, texmidstep;
    uint8_t *map;
    post_t *column;

    halfyfrac = FLOATTOFIXED(halfy);

    baselight = seg->seg->frontside->sector->light >> LIGHTSHIFT;
    if(seg->seg->v1->y == seg->seg->v2->y)
        baselight--;
    else if(seg->seg->v1->x == seg->seg->v2->x)
        baselight++;
    baselight = CLAMP(baselight, 0, LIGHTLEVELS - 1);

    portaltop = FLOATTOFIXED(seg->seg->frontside->sector->ceilheight);
    portalbottom = FLOATTOFIXED(seg->seg->frontside->sector->floorheight);
    if(seg->seg->backside)
    {
        portaltop = FLOATTOFIXED(MIN(seg->seg->frontside->sector->ceilheight, seg->seg->backside->sector->ceilheight));
        portalbottom = FLOATTOFIXED(MAX(seg->seg->frontside->sector->floorheight, seg->seg->backside->sector->floorheight));
    }

    if(seg->seg->line->flags & LINEDEF_LOWERUNPEG)
        ttop = (seg->seg->frontside->mid->h << FIXEDSHIFT) - (portaltop - portalbottom) + FLOATTOFIXED(seg->seg->frontside->yoffs);
    else
        ttop = FLOATTOFIXED(seg->seg->frontside->yoffs);

    scalestepfrac = FLOATTOFIXED(seg->scalestep);
    startx = (x1 - seg->x1) << FIXEDSHIFT;
    scalefrac = FLOATTOFIXED(seg->scale1) + fixedmul(startx, scalestepfrac);

    top = fixedmul(FLOATTOFIXED(viewz) - portaltop, scalefrac) + halfyfrac;
    topstep = fixedmul(FLOATTOFIXED(viewz) - portaltop, scalestepfrac);

    height = fixedmul(portaltop - portalbottom, scalefrac);
    hstep = fixedmul(portaltop - portalbottom, scalestepfrac);

    for(x=x1; x<=x2; x++, top+=topstep, height+=hstep, scalefrac+=scalestepfrac)
    {
        if(seg->maskeds[x - seg->x1] == INT16_MAX)
            continue;
        if(maxscale > 0 && scalefrac >= maxscale)
            continue;
        
        lightindex = CLAMP(FIXEDTOFLOAT(scalefrac) * 16 * 320.0 / screenwidth, 0, SCALEBANDS - 1);
        map = scalemap[baselight][lightindex];

        //iscale = fixeddiv((fixed_t) 1 << FIXEDSHIFT, scalefrac);
        iscale = 0xFFFFFFFFu / (uint32_t) scalefrac;
        texmid = ttop + fixedmul(halfyfrac - top, iscale);
        top = halfyfrac - fixedmul(texmid, scalefrac);

        pxtop = (top >> FIXEDSHIFT) + 1;
        pxbottom = (top + height) >> FIXEDSHIFT;

        if(pxtop <= seg->top[x - seg->x1])
            pxtop = seg->top[x - seg->x1] + 1;
        if(pxbottom >= seg->bottom[x - seg->x1])
            pxbottom = seg->bottom[x - seg->x1] - 1;

        if(pxtop > pxbottom)
            continue;

        column = (post_t*) (tex_getcolumn(seg->seg->frontside->mid, seg->maskeds[x - seg->x1]) - 3);
        render_postcolumn(column, map, x, pxtop, pxbottom, texmid, iscale, scalefrac);
        seg->maskeds[x - seg->x1] = INT16_MAX;
    }
}

void render_clipthing(visthing_t* visthing, int w)
{
    int x;
    drawseg_t *drawseg;

    fixed_t farthest, closest, scale;
    int x1, x2;

    for(drawseg=&drawsegs[ndrawsegs-1]; drawseg>=drawsegs; drawseg--)
    {
        farthest = FLOATTOFIXED(MIN(drawseg->scale1, drawseg->scale2));
        closest = FLOATTOFIXED(MAX(drawseg->scale1, drawseg->scale2));

        x1 = MAX(drawseg->x1, visthing->x);
        x2 = MIN(drawseg->x2, visthing->x + w - 1);

        if(x1 > x2)
            continue;

        if(farthest < visthing->scale && drawseg->maskeds)
            render_maskedseg(drawseg, x1, x2, visthing->scale);

        if(closest <= visthing->scale || drawseg->maskeds)
            continue;

        scale = FLOATTOFIXED(drawseg->scale1) + fixedmul((x1 - drawseg->x1) << FIXEDSHIFT, FLOATTOFIXED(drawseg->scalestep));
        for(x=x1; x<=x2; x++, scale+=FLOATTOFIXED(drawseg->scalestep))
        {
            if(scale <= visthing->scale)
                continue;

            if(!drawseg->top && !drawseg->bottom)
            {
                visthingtop[x] = screenheight;
                visthingbottom[x] = -1;
                continue;
            }
            if(drawseg->top)
                visthingtop[x] = MAX(visthingtop[x], drawseg->top[x - drawseg->x1] + 1);

            if(drawseg->bottom)
                visthingbottom[x] = MIN(visthingbottom[x], drawseg->bottom[x - drawseg->x1] - 1);
        }
    }
}

void render_drawthing(visthing_t* thing)
{
    int x;

    int w, h;
    fixed_t iscale, texmid;
    fixed_t s, sstep;
    int y1, y2;
    post_t *post;

    w = fixedmul((fixed_t) thing->patch->w << FIXEDSHIFT, thing->scale) >> FIXEDSHIFT;
    h = fixedmul((fixed_t) thing->patch->h << FIXEDSHIFT, thing->scale) >> FIXEDSHIFT;

    if(w <= 0 || h <= 0)
        return;

    iscale = fixeddiv((fixed_t) 1 << FIXEDSHIFT, thing->scale);
    texmid = fixedmul(FLOATTOFIXED(halfy) - (thing->y << FIXEDSHIFT), iscale);

    for(x=MAX(thing->x, 0); x<thing->x+w&&x<screenwidth; x++)
    {
        visthingtop[x] = -1;
        visthingbottom[x] = screenheight;
    }

    render_clipthing(thing, w);

    if(thing->mirror)
    {
        s = ((fixed_t) thing->patch->w << FIXEDSHIFT) - 1;
        sstep = -iscale;
    }
    else
    {
        s = 0;
        sstep = iscale;
    }

    x = thing->x;
    if(x < 0)
    {
        s += fixedmul(sstep, (fixed_t) (-x) << FIXEDSHIFT);
        x = 0;
    }

    for(; x<thing->x+w&&x<screenwidth; x++, s+=sstep)
    {
        y1 = MAX(thing->y, visthingtop[x] + 1);
        y2 = MIN(thing->y + h - 1, visthingbottom[x] - 1);
        if(y1 > y2)
            continue;
        
        post = (post_t*) ((uint8_t*) thing->patch + thing->patch->postoffs[s >> FIXEDSHIFT]);
        render_postcolumn(post, thing->colormap, x, y1, y2, texmid, iscale, thing->scale);
    }
}

void render_sortthings(void)
{
    int i;

    static visthing_t unsorted;
    static visthing_t sorted;

    visthing_t *vt, *best, *it;

    if(nvisthings <= 0)
    {
        visthinghead = visthingtail = NULL;
        return;
    }

    unsorted.next = &visthings[0];
    unsorted.prev = &visthings[nvisthings - 1];

    for(i = 0; i < nvisthings; i++)
    {
        visthings[i].prev = (i == 0) ? &unsorted : &visthings[i - 1];
        visthings[i].next = (i == nvisthings - 1) ? &unsorted : &visthings[i + 1];
    }

    sorted.next = sorted.prev = &sorted;

    if(nvisthings < 2)
    {
        visthinghead = &visthings[0];
        visthingtail = &visthings[0];
        visthings[0].prev = NULL;
        visthings[0].next = NULL;
        return;
    }

    for(i=0; i<nvisthings; i++)
    {
        best = unsorted.next;

        for(it=unsorted.next; it!=&unsorted; it=it->next)
            if(it->scale > best->scale)
                best = it;

        best->prev->next = best->next;
        best->next->prev = best->prev;

        best->prev = sorted.prev;
        best->next = &sorted;
        sorted.prev->next = best;
        sorted.prev = best;
    }

    visthinghead = sorted.next;
    visthingtail = sorted.prev;

    visthinghead->prev = NULL;
    visthingtail->next = NULL;
}

void render_drawthings(void)
{
    int i;
    drawseg_t *drawseg;
    visthing_t *visthing;

    render_sortthings();
    for(visthing=visthingtail; visthing; visthing=visthing->prev)
        render_drawthing(visthing);

    for(drawseg=&drawsegs[ndrawsegs-1]; drawseg>=drawsegs; drawseg--)
    {
        if(!drawseg->maskeds)
            continue;
        render_maskedseg(drawseg, drawseg->x1, drawseg->x2, 0);
    }
}

void render_drawspan(visplane_t* plane, int y, int x1, int x2)
{
    int x;

    color_t color;
    float dist;
    float worldx, worldy;
    float dx, dy, step;
    float adjust;
    int s, t;
    angle_t a1;
    int lightindex;
    uint8_t *map;

    dist = plane->z / ANGTAN(render_ytoangle(y));

    lightindex = dist / 16.0;
    lightindex = CLAMP(lightindex, 0, ZBANDS - 1);
    map = zmap[plane->baselight][lightindex];

    worldx = viewx;
    worldy = viewy;

    a1 = viewangle + render_xtoangle(x1);
    adjust = 1.0 / ANGCOS(a1 - viewangle);
    worldx += ANGCOS(a1) * dist * adjust;
    worldy += ANGSIN(a1) * dist * adjust;

    step = dist / projectconst;

    dx = ANGSIN(viewangle) * step;
    dy = -ANGCOS(viewangle) * step;

    for(x=x1; x<=x2; x++, worldx+=dx, worldy+=dy)
    {
        s = (int) worldx & (FLAT_RES - 1);
        t = (int) worldy & (FLAT_RES - 1);
        color = palette[map[((uint8_t*) plane->flat->cache)[t * FLAT_RES + s]]];
        pixels[y * screenwidth + x] = (int) color.r << 16 | (int) color.g << 8 | (int) color.b;
    }
}

void render_solidcol(uint8_t* col, uint8_t* colmap, int texheight, int x, int y1, int y2, float t, float tstep)
{
    int y, dst;

    color_t color;
    fixed_t tfrac, tsfrac;

    tfrac = FLOATTOFIXED(t);
    while(tfrac<0)
        tfrac += texheight << FIXEDSHIFT;
    tsfrac = FLOATTOFIXED(tstep);

    for(y=y1, dst=y1*screenwidth+x; y<=y2; y++, tfrac+=tsfrac, dst+=screenwidth)
    {
        color = palette[colmap[col[(tfrac >> FIXEDSHIFT) % texheight]]];
        pixels[dst] = (int) color.r << 16 | (int) color.g << 8 | (int) color.b;
    }
}

void render_drawskyplane(visplane_t* plane)
{
    int x;

    int top, bottom;
    angle_t a;
    int s;
    uint8_t *col;
    float tstep;

    tex_stitch(levelskytex);

    tstep = 320.0 / screenheight / 2;
    for(x=plane->x1; x<=plane->x2; x++)
    {
        if((plane->tops[x] == -1 && plane->bottoms[x] == -1) || plane->tops[x] > plane->bottoms[x])
            continue;

        top = CLAMP(plane->tops[x], 0, screenheight - 1);
        bottom = CLAMP(plane->bottoms[x], 0, screenheight - 1);

        a = render_xtoangle(x) + viewangle;
        s = (float) a / (float) ANG90 * (float) SKYW;

        col = tex_getcolumn(levelskytex, s);
        render_solidcol(col, colormap->maps[0], SKYH, x, top, bottom, halfy + (top - halfy) * tstep, tstep);
    }
}

void render_drawplane(visplane_t* plane)
{
    int x, y;

    if(plane->flat == skylump)
    {
        render_drawskyplane(plane);
        return;
    }

    wad_cache(plane->flat);

    x = plane->x1;
    while(x <= plane->x2 && (plane->tops[x] == -1 || plane->bottoms[x] == -1))
        x++;
    if(x > plane->x2)
        return;

    for(y=plane->tops[x]; y<=plane->bottoms[x]; y++)
        spanstarts[y] = x;

    for(x++; x<=plane->x2; x++)
    {
        if(plane->tops[x] == -1 || plane->bottoms[x] == -1)
        {
            if(plane->tops[x-1] != -1 && plane->bottoms[x-1] != -1)
                for(y=plane->tops[x-1]; y<=plane->bottoms[x-1]; y++)
                    render_drawspan(plane, y, spanstarts[y], x-1);
            continue;
        }

        if(plane->tops[x-1] == -1 || plane->bottoms[x-1] == -1)
        {
            for(y=plane->tops[x]; y<=plane->bottoms[x]; y++)
                spanstarts[y] = x;
            continue;
        }

        if(plane->tops[x] > plane->tops[x-1])
        {
            for(y=plane->tops[x-1]; y<=MIN(plane->tops[x] - 1, plane->bottoms[x-1]); y++)
                render_drawspan(plane, y, spanstarts[y], x-1);
        }
        else if(plane->tops[x] < plane->tops[x-1])
        {
            for(y=plane->tops[x]; y<=MIN(plane->tops[x-1] - 1, plane->bottoms[x]); y++)
                spanstarts[y] = x;
        }

        if(plane->bottoms[x] < plane->bottoms[x-1])
        {
            for(y=MAX(plane->bottoms[x] + 1, plane->tops[x-1]); y<=plane->bottoms[x-1]; y++)
                render_drawspan(plane, y, spanstarts[y], x-1);
        }
        else if(plane->bottoms[x] > plane->bottoms[x-1])
        {
            for(y=plane->bottoms[x-1]+1; y<=plane->bottoms[x]; y++)
                spanstarts[y] = x;
        }
    }

    if(plane->tops[x-1] != -1 && plane->bottoms[x-1] != -1)
        for(y=plane->tops[x-1]; y<=plane->bottoms[x-1]; y++)
            render_drawspan(plane, y, spanstarts[y], x-1);
}

void render_drawplanes(void)
{
    int i;

    for(i=0; i<nvisplanes; i++)
        render_drawplane(&visplanes[i]);
}

// split the plane if geometry is incompatible
visplane_t* render_splitplane(visplane_t* plane, int x1, int x2)
{
    int x;

    int intersect1, intersect2;

    if(plane->x1 == -1 || plane->x2 == -1)
    {
        plane->x1 = x1;
        plane->x2 = x2;
        for(x=x1; x<=x2; x++)
            plane->tops[x] = plane->bottoms[x] = -1;
        return plane;
    }

    if(x2 < plane->x1 || x1 > plane->x2)
    {
        if(x2 < plane->x1 - 1)
            for(x=x2+1; x<plane->x1; x++)
                plane->tops[x] = plane->bottoms[x] = -1;
        if(x1 > plane->x2 + 1)
            for(x=plane->x2+1; x<x1; x++)
                plane->tops[x] = plane->bottoms[x] = -1;

        plane->x1 = MIN(x1, plane->x1);
        plane->x2 = MAX(x2, plane->x2);
        return plane;
    }

    intersect1 = MAX(x1, plane->x1);
    intersect2 = MIN(x2, plane->x2);

    for(x=intersect1; x<=intersect2; x++)
    {
        if(plane->bottoms[x] == -1 && plane->tops[x] == -1)
            continue;
        
        if(nvisplanes >= MAX_VISPLANE)
        {
            fprintf(stderr, "render_splitplane: MAX_VISPLANE reached\n");
            return NULL;
        }

        visplanes[nvisplanes].x1 = x1;
        visplanes[nvisplanes].x2 = x2;
        visplanes[nvisplanes].z = plane->z;
        visplanes[nvisplanes].flat = plane->flat;
        visplanes[nvisplanes].baselight = plane->baselight;
        return &visplanes[nvisplanes++];
    }

    plane->x1 = MIN(x1, plane->x1);
    plane->x2 = MAX(x2, plane->x2);
    return plane;
}

visplane_t* render_getvisplane(float z, lumpinfo_t* flat, int light)
{
    int i;

    // these aren't a concern for merging sky planes
    if(flat == skylump)
        z = light = 0;

    for(i=nvisplanes-1; i>=0; i--)
        if(visplanes[i].z == z && visplanes[i].flat == flat && visplanes[i].baselight == light)
            return &visplanes[i];

    if(nvisplanes >= MAX_VISPLANE)
    {
        fprintf(stderr, "render_splitplane: MAX_VISPLANE reached\n");
        return NULL;
    }
    
    visplanes[nvisplanes].x1 = visplanes[nvisplanes].x2 = -1;
    visplanes[nvisplanes].z = z;
    visplanes[nvisplanes].flat = flat;
    visplanes[nvisplanes].baselight = light;
    return &visplanes[nvisplanes++];
}

void render_segrange(int x1, int x2, seg_t* seg)
{
    int x, y;

    int color;
    float portaltop, portalbottom, worldtop, worldbottom;
    float dist, scale, scale1, scale2, scalestep;
    float top, topstep, height, hstep;
    float tsilheight, portalheight, bsilheight, tsilstep, bsilstep;
    int pltop, plbot;
    float sbase, s;
    angle_t normal, a1, a2, a;
    int pxtop, pxbottom;
    float ftop, fbottom;
    float ttop, t, tstep;
    int mods, modt;
    bool drawceil, drawfloor, drawtop, drawbottom, drewtop, drewbottom;
    visplane_t *floorplane, *ceilplane;
    drawseg_t *drawseg, *maskedseg;
    uint8_t *column;
    int sectorlight, baselight, lightindex;
    uint8_t *map;
    
    a1 = render_xtoangle(x1) + viewangle;
    a2 = render_xtoangle(x2) + viewangle;

    normal = seg->angle + ANG90;
    // perpendicular distance from line to camera
    dist = magnitude(seg->v1->x - viewx, seg->v1->y - viewy) * ANGCOS(unclippeda1 - normal);

    scale = scale1 = ANGCOS(a1 - normal) / MAX(dist * ANGCOS(a1 - viewangle), 0.05) * projectconst;
    scale2 = ANGCOS(a2 - normal) / MAX(dist * ANGCOS(a2 - viewangle), 0.05) * projectconst;

    scalestep = (scale2 - scale1) / (float) (x2 - x1);

    worldtop = portaltop = seg->frontside->sector->ceilheight;
    worldbottom = portalbottom = seg->frontside->sector->floorheight;

    if(seg->backside && seg->backside->sector)
    {
        portaltop = MIN(portaltop, seg->backside->sector->ceilheight);
        portalbottom = MAX(portalbottom, seg->backside->sector->floorheight);
    }

    if(seg->frontside->sector->ceiltex == skylump && seg->backside->sector->ceiltex == skylump)
    {
        worldtop = portaltop;
    }

    drawtop = worldtop > portaltop;
    drawbottom = worldbottom < portalbottom;

    if(seg->frontside->mid)
        tex_stitch(seg->frontside->mid);
    if(drawtop && seg->frontside->upper)
        tex_stitch(seg->frontside->upper);
    if(drawbottom && seg->frontside->lower)
        tex_stitch(seg->frontside->lower);

    top = -(worldtop - viewz) * scale1 + halfy;
    topstep = -(worldtop - viewz) * scalestep;

    height = (worldtop - worldbottom) * scale1;
    hstep = (worldtop - worldbottom) * scalestep;

    tsilheight = bsilheight = 0;
    if(drawtop)
    {
        tsilheight = (worldtop - portaltop) * scale1;
        tsilstep = (worldtop - portaltop) * scalestep;
    }
    if(drawbottom)
    {
        bsilheight = (portalbottom - worldbottom) * scale1;
        bsilstep = (portalbottom - worldbottom) * scalestep;
    }

    sectorlight = baselight = seg->frontside->sector->light >> LIGHTSHIFT;
    if(seg->v1->y == seg->v2->y)
        baselight--;
    else if(seg->v1->x == seg->v2->x)
        baselight++;
    sectorlight = CLAMP(sectorlight, 0, LIGHTLEVELS - 1);
    baselight = CLAMP(baselight, 0, LIGHTLEVELS - 1);

    drawceil = true;
    if(seg->frontside->sector->ceilheight <= viewz && seg->frontside->sector->ceiltex != skylump)
        drawceil = false;
    if(seg->backside 
    && seg->backside->sector->ceilheight == seg->frontside->sector->ceilheight
    && seg->backside->sector->ceiltex == seg->frontside->sector->ceiltex
    && seg->backside->sector->floorheight < seg->backside->sector->ceilheight
    && seg->backside->sector->light == seg->frontside->sector->light)
        drawceil = false;

    drawfloor = true;
    if(seg->frontside->sector->floorheight >= viewz)
        drawfloor = false;
    if(seg->backside 
    && seg->backside->sector->floorheight == seg->frontside->sector->floorheight
    && seg->backside->sector->floortex == seg->frontside->sector->floortex
    && seg->backside->sector->ceilheight > seg->backside->sector->floorheight
    && seg->backside->sector->light == seg->frontside->sector->light)
        drawfloor = false;
    
    floorplane = ceilplane = NULL;
    if(drawceil)
    {
        ceilplane = render_getvisplane(seg->frontside->sector->ceilheight - viewz, seg->frontside->sector->ceiltex, sectorlight);
        ceilplane = render_splitplane(ceilplane, x1, x2);
    }
    if(drawfloor)
    {
        floorplane = render_getvisplane(seg->frontside->sector->floorheight - viewz, seg->frontside->sector->floortex, sectorlight);
        floorplane = render_splitplane(floorplane, x1, x2);
    }

    maskedseg = NULL;
    if(seg->backside && seg->frontside->mid)
    {
        if(ndrawsegs >= MAX_DRAWSEG)
            return;
        if(clipend + (x2 + 1 - x1) * 3 >= clipbuff + clipbuffsize)
            return;

        maskedseg = &drawsegs[ndrawsegs++];

        maskedseg->seg = seg;
        maskedseg->x1 = x1;
        maskedseg->x2 = x2;

        maskedseg->scale1 = scale1;
        maskedseg->scale2 = scale2;
        maskedseg->scalestep = scalestep;
        maskedseg->top = clipend;
        clipend += x2 + 1 - x1;
        maskedseg->bottom = clipend;
        clipend += x2 + 1 - x1;
        maskedseg->maskeds = clipend;
        clipend += x2 + 1 - x1;
    }

    sbase = seg->frontside->xoffs + seg->offset;

    drewtop = drewbottom = false;
    for(x=x1; x<=x2; x++, top+=topstep, height+=hstep, scale+=scalestep)
    {
        tstep = 1.0 / scale;

        a = render_xtoangle(x);
        s = sbase + dist * (ANGTAN(unclippeda1 - normal) - ANGTAN(a + viewangle - normal));
        
        lightindex = CLAMP(scale * 16 * 320.0 / screenwidth, 0, SCALEBANDS - 1);
        map = scalemap[baselight][lightindex];

        if(ceilplane)
        {
            pltop = topclips[x] + 1;
            plbot = top;

            if(plbot >= bottomclips[x])
                plbot = bottomclips[x] - 1;

            if(pltop <= plbot)
            {
                ceilplane->tops[x] = CLAMP(pltop, 0, screenheight - 1);
                ceilplane->bottoms[x] = CLAMP(plbot, 0, screenheight - 1);
            }
            else
                ceilplane->tops[x] = ceilplane->bottoms[x] = -1;
        }

        if(floorplane)
        {
            pltop = top + height + 1;
            plbot = bottomclips[x] - 1;

            if(pltop <= topclips[x])
                pltop = topclips[x] + 1;

            if(pltop <= plbot)
            {
                floorplane->tops[x] = CLAMP(pltop, 0, screenheight - 1);
                floorplane->bottoms[x] = CLAMP(plbot, 0, screenheight - 1);
            }
            else
                floorplane->tops[x] = floorplane->bottoms[x] = -1;
        }

        if(portaltop > portalbottom)
        {   
            ftop = top + tsilheight;
            fbottom = top + height - bsilheight;

            pxtop = ftop + 1;
            pxbottom = fbottom;

            if(pxtop <= topclips[x])
                pxtop = topclips[x] + 1;
            if(pxbottom >= bottomclips[x])
                pxbottom = bottomclips[x] - 1;

            if(!drawtop)
            {
                if(drawceil)
                    topclips[x] = pxtop - 1;
                drewtop = seg->backside != NULL;
            }

            if(!drawbottom) 
            {
                if(drawfloor)
                    bottomclips[x] = pxbottom + 1;
                drewbottom = seg->backside != NULL;
            }

            if(!seg->backside)
            {
                topclips[x] = screenheight;
                bottomclips[x] = -1;

                if(pxtop <= pxbottom && seg->frontside->mid)
                {
                    if(seg->line->flags & LINEDEF_LOWERUNPEG)
                        ttop = seg->frontside->mid->h - (portaltop - portalbottom) + seg->frontside->yoffs;
                    else
                        ttop = seg->frontside->yoffs;

                    t = tstep * ((float) pxtop - ftop) + ttop;
                    column = tex_getcolumn(seg->frontside->mid, s);
                    render_solidcol(column, map, seg->frontside->mid->h, x, pxtop, pxbottom, t, tstep);
                }
            }
            else if(pxtop <= pxbottom && seg->frontside->mid)
                maskedseg->maskeds[x - x1] = s;
        }

        if(drawtop)
        {
            ftop = top;
            fbottom = top + tsilheight;
            tsilheight += tsilstep;

            pxtop = ftop + 1;
            pxbottom = fbottom;

            if(pxtop <= topclips[x])
                pxtop = topclips[x] + 1;
            if(pxbottom >= bottomclips[x])
                pxbottom = bottomclips[x] - 1;

            if(pxbottom > topclips[x])
            {
                topclips[x] = pxbottom;
                drewtop = true;
            }

            if(pxtop <= pxbottom && seg->frontside->upper)
            {
                if(seg->line->flags & LINEDEF_UPPERUNPEG)
                    ttop = seg->frontside->yoffs;
                else
                    ttop = seg->frontside->upper->h - (worldtop - portaltop) + seg->frontside->yoffs;

                t = tstep * ((float) pxtop - ftop) + ttop;
                column = tex_getcolumn(seg->frontside->upper, s);
                render_solidcol(column, map, seg->frontside->upper->h, x, pxtop, pxbottom, t, tstep);
            }
        }

        if(drawbottom)
        {
            ftop = top + height - bsilheight;
            fbottom = top + height;
            bsilheight += bsilstep;

            pxtop = ftop + 1;
            pxbottom = fbottom;

            if(pxtop <= topclips[x])
                pxtop = topclips[x] + 1;
            if(pxbottom >= bottomclips[x])
                pxbottom = bottomclips[x] - 1;
            
            if(pxtop < bottomclips[x])
            {
                bottomclips[x] = pxtop;
                drewbottom = true;
            }

            if(pxtop <= pxbottom)
            {
                if(seg->frontside->lower)
                {
                    if(seg->line->flags & LINEDEF_LOWERUNPEG)
                        ttop = worldtop - portalbottom + seg->frontside->yoffs;
                    else
                        ttop = seg->frontside->yoffs;

                    t = tstep * ((float) pxtop - ftop) + ttop;
                    column = tex_getcolumn(seg->frontside->lower, s);
                    render_solidcol(column, map, seg->frontside->lower->h, x, pxtop, pxbottom, t, tstep);
                }
            }
        }
    }

    if(!seg->backside || seg->backside->sector->floorheight == seg->backside->sector->ceilheight)
    {
        if(ndrawsegs >= MAX_DRAWSEG)
            return;

        drawseg = &drawsegs[ndrawsegs++];

        drawseg->seg = seg;
        drawseg->x1 = x1;
        drawseg->x2 = x2;
        drawseg->scale1 = scale1;
        drawseg->scale2 = scale2;
        drawseg->scalestep = scalestep;
        drawseg->top = drawseg->bottom = drawseg->maskeds = NULL;
    }

    if(drewtop)
    {
        if(ndrawsegs >= MAX_DRAWSEG)
            return;
        if(clipend + (x2 + 1 - x1) >= clipbuff + clipbuffsize)
            return;

        drawseg = &drawsegs[ndrawsegs++];

        drawseg->seg = seg;
        drawseg->x1 = x1;
        drawseg->x2 = x2;
        drawseg->scale1 = scale1;
        drawseg->scale2 = scale2;
        drawseg->scalestep = scalestep;
        drawseg->bottom = drawseg->maskeds = NULL;
        drawseg->top = clipend;
        clipend += x2 + 1 - x1;

        memcpy(drawseg->top, topclips + x1, (x2 + 1 - x1) * sizeof(int16_t));
    }

    if(drewbottom)
    {
        if(ndrawsegs >= MAX_DRAWSEG)
            return;
        if(clipend + (x2 + 1 - x1) >= clipbuff + clipbuffsize)
            return;

        drawseg = &drawsegs[ndrawsegs++];

        drawseg->seg = seg;
        drawseg->x1 = x1;
        drawseg->x2 = x2;
        drawseg->scale1 = scale1;
        drawseg->scale2 = scale2;
        drawseg->scalestep = scalestep;
        drawseg->top = drawseg->maskeds = NULL;
        drawseg->bottom = clipend;
        clipend += x2 + 1 - x1;

        memcpy(drawseg->bottom, bottomclips + x1, (x2 + 1 - x1) * sizeof(int16_t));
    }

    if(maskedseg)
    {
        memcpy(maskedseg->top, topclips + x1, (x2 + 1 - x1) * sizeof(int16_t));
        memcpy(maskedseg->bottom, bottomclips + x1, (x2 + 1 - x1) * sizeof(int16_t));
    }
}

void render_clipandaddseg(int x1, int x2, seg_t *seg)
{
    int i, j;

    for(i=0; x2>=clipspans[i].x1-1 && x1<=x2; i++)
    {
        if(clipspans[i].x2 < x1 - 1)
            continue;

        if(x1 < clipspans[i].x1)
        {
            render_segrange(x1, clipspans[i].x1 - 1, seg);
            clipspans[i].x1 = x1;

            if(clipspans[i-1].x2 >= clipspans[i].x1 - 1)
            {
                clipspans[i-1].x2 = clipspans[i].x2;
                
                nclipspans--;
                for(j=i; j<nclipspans; j++)
                    clipspans[j] = clipspans[j+1];
                i--;
            }
        }

        x1 = clipspans[i].x2 + 1;
    }

    if(x1 <= x2)
    {
        render_segrange(x1, x2, seg);

        if(clipspans[i-1].x2 >= x1 - 1)
        {
            clipspans[i-1].x2 = x2;
        }
        else
        {
            if(nclipspans >= MAX_CLIPSPAN)
            {
                fprintf(stderr, "render_clipandaddseg: MAX_CLIPSPAN reached\n");
                return;
            }

            nclipspans++;
            for(j = nclipspans - 1; j > i; j--)
                clipspans[j] = clipspans[j-1];
            
            clipspans[i].x1 = x1;
            clipspans[i].x2 = x2;
        }
    }
}
void render_clipseg(int x1, int x2, seg_t* seg)
{
    int i;
    for(i=0; x2>=clipspans[i].x1 && x1<=x2; i++)
    {
        if(clipspans[i].x2 < x1)
            continue;

        if(x1 < clipspans[i].x1)
            render_segrange(x1, clipspans[i].x1-1, seg);

        x1 = clipspans[i].x2+1;
    }

    if(x1 <= x2)
        render_segrange(x1, x2, seg);
}

void render_seg(seg_t* seg)
{
    angle_t a1, a2, theta;
    int x1, x2;

    a1 = unclippeda1 = ANGATAN2(seg->v1->y - viewy, seg->v1->x - viewx);
    a2 = ANGATAN2(seg->v2->y - viewy, seg->v2->x - viewx);

    theta = a1 - a2;
    if(theta >= ANG180-1)
        return;

    a1 -= viewangle;
    a2 -= viewangle;

    if(a1 < (ANG360-HFOV/2) && a1 > (HFOV/2) && a1 - (HFOV/2) >= theta)
        return;
    if(a2 > (HFOV/2) && a2 < (ANG360-HFOV/2) && (ANG360-HFOV/2) - a2 >= theta)
        return;

    if(a1 < (ANG360-HFOV/2) && a1 > HFOV/2)
        a1 = HFOV/2;
    if(a2 > HFOV/2 && a2 < (ANG360-HFOV/2))
        a2 = -HFOV/2;
    
    x1 = render_angletox(a1);
    x2 = render_angletox(a2) - 1; // so we don't double up where segs meet

    if(x1 > x2)
        return;

    // solid wall
    if(!seg->backside)
        return render_clipandaddseg(x1, x2, seg);

    // closed door
    if(seg->backside->sector->ceilheight <= seg->frontside->sector->floorheight
    || seg->backside->sector->floorheight >= seg->frontside->sector->ceilheight)
        return render_clipandaddseg(x1, x2, seg);

    // no new visplanes or walls need to be drawn, skip
    if(!seg->frontside->mid
    && seg->frontside->sector->ceilheight == seg->backside->sector->ceilheight
    && seg->frontside->sector->floorheight == seg->backside->sector->floorheight
    && seg->frontside->sector->ceiltex == seg->backside->sector->ceiltex
    && seg->frontside->sector->floortex == seg->backside->sector->floortex
    && seg->frontside->sector->light == seg->backside->sector->light)
        return;

    // window or something
    render_clipseg(x1, x2, seg);
}

// returns true if it was culled
bool render_visthinginfo(object_t* mobj, visthing_t* visthing)
{
    fixed_t sinview, cosview;
    fixed_t dx, dy, dz, dist, centerx, centery;
    int x, y, w, h;
    pic_t *pic;
    sprite_t *sprite;
    sprframe_t *frame;
    lumpinfo_t *patch;
    angle_t a, theta;
    int rotframe;
    int lightindex;

    sinview = FLOATTOFIXED(ANGSIN(viewangle));
    cosview = FLOATTOFIXED(ANGCOS(viewangle));

    dx = FLOATTOFIXED(mobj->info.x - viewx);
    dy = FLOATTOFIXED(mobj->info.y - viewy);
    dz = FLOATTOFIXED(mobj->info.z - viewz);

    dist = fixedmul(dx, cosview) + fixedmul(dy, sinview);
    if(dist < 1 << FIXEDSHIFT)
        return true;

    visthing->scale = fixeddiv(projectfrac, dist);
    
    centerx = FLOATTOFIXED(halfx) + fixedmul(fixedmul(dx, sinview) - fixedmul(dy, cosview), visthing->scale);
    centery = FLOATTOFIXED(halfy) - fixedmul(dz, visthing->scale);

    if(states[mobj->info.state].frame & 0x8000)
    {
        visthing->colormap = colormap->maps[0];
    }
    else
    {
        lightindex = CLAMP(FIXEDTOFLOAT(visthing->scale * 16) * 320.0 / screenwidth, 0, SCALEBANDS - 1);
        visthing->colormap = scalemap[mobj->ssector->sector->light >> LIGHTSHIFT][lightindex];
    }

    sprite = &sprites[states[mobj->info.state].sprite];
    if((states[mobj->info.state].frame & 0x7FFF) >= sprite->nframes)
        return true;
    frame = &sprite->frames[states[mobj->info.state].frame & 0x7FFF];

    rotframe = 0;
    if(frame->rotational)
    {
        a = ANGATAN2(dy, dx);
        theta = a - mobj->info.angle + ANG45 / 2;
        rotframe = (theta + ANG180) / ANG45;
    }

    if(frame->rotlumps[rotframe] < 0)
        return true;

    patch = &lumps[frame->rotlumps[rotframe]];
    visthing->mirror = frame->mirror[rotframe];

    wad_cache(patch);
    visthing->patch = pic = patch->cache;

    x = (centerx - fixedmul((fixed_t) pic->xoffs << FIXEDSHIFT, visthing->scale)) >> FIXEDSHIFT;
    y = (centery - fixedmul((fixed_t) pic->yoffs << FIXEDSHIFT, visthing->scale)) >> FIXEDSHIFT;

    if(x >= screenwidth || y >= screenheight)
        return true;

    w = fixedmul((fixed_t) pic->w << FIXEDSHIFT, visthing->scale) >> FIXEDSHIFT;
    h = fixedmul((fixed_t) pic->h << FIXEDSHIFT, visthing->scale) >> FIXEDSHIFT;

    if(x + w < 0 || y + h < 0)
        return true;

    visthing->x = x;
    visthing->y = y;

    return false;
}

void render_sectorthings(sector_t* sector)
{
    object_t *mobj;
    visthing_t *visthing;

    if(sector->frameindex == frameindex)
        return;
    sector->frameindex = frameindex;

    for(mobj=sector->mobjs; mobj; mobj=mobj->snext)
    {
        if(nvisthings >= MAX_VISTHING)
            break;

        if(mobj == player.mobj)
            continue;

        visthing = &visthings[nvisthings++];
        if(render_visthinginfo(mobj, visthing))
        {
            nvisthings--;
            continue;
        }

        visthing->prev = visthing->next = NULL;
    }
}

void render_subsector(int issector)
{
    int i;

    ssector_t *ssector;

    ssector = &ssectors[issector];
    
    render_sectorthings(ssector->sector);

    for(i=ssector->firstseg; i<ssector->firstseg+ssector->nsegs; i++)
        render_seg(&segs[i]);
}

void render_node(int inode)
{
    node_t *node;
    int side;

    if(inode & 0x8000)
        return render_subsector(inode & 0x7FFF);
    
    node = &nodes[inode];
    side = level_nodeside(node, viewx, viewy);

    render_node(node->children[side]);
    render_node(node->children[!side]);
}

void render_setup(void)
{
    int i, x;

    nclipspans = 2;
    clipspans[0].x1 = INT16_MIN;
    clipspans[0].x2 = -1;
    clipspans[1].x1 = screenwidth;
    clipspans[1].x2 = INT16_MAX;

    for(i=0; i<screenwidth; i++)
    {
        topclips[i] = -1;
        bottomclips[i] = screenheight;
    }

    nvisplanes = 0;

    nvisthings = 0;
    visthinghead = NULL;

    ndrawsegs = 0;
    clipend = clipbuff;
}

void render(void)
{
    render_setup();
    render_node(nnodes-1);   
    render_drawplanes();
    render_drawthings();

    frameindex++;
}