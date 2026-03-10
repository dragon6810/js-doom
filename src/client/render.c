#include "render.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "draw.h"
#include "level.h"
#include "screen.h"
#include "tex.h"
#include "visweapon.h"
#include "wad.h"

#define MAX_CLIPSPAN 192
#define MAX_VISPLANE 256
#define MAX_DRAWSEG  512
#define MAX_VISTHING 256
#define FLAT_RES 64

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
    mc_e color;

    struct visthing_s *next, *prev;
} visthing_t;

typedef struct
{
    seg_t *seg;
    int16_t *bottom, *top; // both NULL, solid wall. one null, top or bottom wall
    int16_t *maskeds;
    fixed_t scale1, scale2;
    fixed_t scalestep;
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

fixed_t viewxfrac, viewyfrac, viewzfrac;

const int npastelcolors = 10;
const int pastelcolors[] = { 16, 51, 83, 115, 161, 171, 194, 211, 226, 250 };

// 180 degrees, index 0 is actually at -90 deg
int angletox[TABANGLES / 2];
angle_t *xtoangle = NULL;

void render_init(void)
{
    const float halfplane = HPLANE / 2;

    int i, j;

    int baselevel, level;
    float scale, tangent;
    angle_t ang;

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

    spanstarts = malloc(screens[SCR_LVL].h * sizeof(int16_t));

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
            scale = 160.0 / (float) (j + 1);
            level = baselevel - scale / 2;
            level = CLAMP(level, 0, LIGHTMAP - 1);
            zmap[i][j] = colormap->maps[level];
        }
    }

    for(i=0; i<TABANGLES/2; i++)
    {
        ang = (i << TABSHIFT) - ANG90;
        if(ang < ANG180 && ang > HFOV/2)
            angletox[i] = -1;
        else if(ang > ANG180 && ang < ANG360-HFOV/2)
            angletox[i] = screenwidth;
        else
            angletox[i] = CLAMP(-ANGTAN(ang) * projectconst + halfx, -1, screenwidth);
    }

    xtoangle = calloc(screenwidth, sizeof(angle_t));
    for(i=0; i<screenwidth; i++)
    {
        for(j=0; j<TABANGLES/2&&angletox[j]>i; j++);
        xtoangle[i] = (j << TABSHIFT) - ANG90;
    }
}

angle_t render_ytoangle(int y)
{
    const float halfplane = VPLANE / 2.0f;

    float alpha = (halfy - y) / halfy;
    float tangent = alpha * halfplane;
    
    return ANGATAN(tangent);
}

void render_maskedseg(drawseg_t* seg, int x1, int x2, fixed_t maxscale)
{
    int x;

    fixed_t halfyfrac;
    fixed_t scale, startx, top, topstep, height, hstep, iscale;
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
    texmid = ttop + portaltop - viewzfrac;

    startx = (x1 - seg->x1) << FIXEDSHIFT;
    scale = seg->scale1 + fixedmul(startx, seg->scalestep);

    top = fixedmul(viewzfrac - portaltop, scale) + halfyfrac;
    topstep = fixedmul(viewzfrac - portaltop, seg->scalestep);

    height = fixedmul(portaltop - portalbottom, scale);
    hstep = fixedmul(portaltop - portalbottom, seg->scalestep);

    for(x=x1; x<=x2; x++, top+=topstep, height+=hstep, scale+=seg->scalestep)
    {
        if(seg->maskeds[x - seg->x1] == INT16_MAX)
            continue;
        if(maxscale > 0 && scale >= maxscale)
            continue;
        
        lightindex = CLAMP(FIXEDTOFLOAT(scale) * 16 * 320.0 / screenwidth, 0, SCALEBANDS - 1);
        map = scalemap[baselight][lightindex];

        iscale = 0xFFFFFFFFu / (uint32_t) scale + 1;

        pxtop = (top >> FIXEDSHIFT) + 1;
        pxbottom = (top + height) >> FIXEDSHIFT;

        if(pxtop <= seg->top[x - seg->x1])
            pxtop = seg->top[x - seg->x1] + 1;
        if(pxbottom >= seg->bottom[x - seg->x1])
            pxbottom = seg->bottom[x - seg->x1] - 1;

        if(pxtop > pxbottom)
            continue;

        column = (post_t*) (tex_getcolumn(seg->seg->frontside->mid, seg->maskeds[x - seg->x1]) - 3);
        draw_postcolumn(column, map, x, pxtop, pxbottom, texmid, iscale, scale);
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
        farthest = MIN(drawseg->scale1, drawseg->scale2);
        closest = MAX(drawseg->scale1, drawseg->scale2);

        x1 = MAX(drawseg->x1, visthing->x);
        x2 = MIN(drawseg->x2, visthing->x + w - 1);

        if(x1 > x2)
            continue;

        if(farthest < visthing->scale && drawseg->maskeds)
            render_maskedseg(drawseg, x1, x2, visthing->scale);

        if(closest <= visthing->scale || drawseg->maskeds)
            continue;

        scale = drawseg->scale1 + fixedmul((x1 - drawseg->x1) << FIXEDSHIFT, drawseg->scalestep);
        for(x=x1; x<=x2; x++, scale+=drawseg->scalestep)
        {
            if(scale <= visthing->scale)
                continue;

            if(!drawseg->top && !drawseg->bottom)
            {
                visthingtop[x] = screens[SCR_LVL].h;
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

    iscale = 0xFFFFFFFFu / (uint32_t) thing->scale + 1;
    texmid = fixedmul(FLOATTOFIXED(halfy) - (thing->y << FIXEDSHIFT), iscale);

    for(x=MAX(thing->x, 0); x<thing->x+w&&x<screenwidth; x++)
    {
        visthingtop[x] = 0;
        visthingbottom[x] = screens[SCR_LVL].h-1;
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
        y1 = MAX(thing->y, visthingtop[x]);
        y2 = MIN(thing->y + h - 1, visthingbottom[x]);
        if(y1 > y2)
            continue;
        
        post = (post_t*) ((uint8_t*) thing->patch + thing->patch->postoffs[s >> FIXEDSHIFT]);
        if(!thing->color)
            draw_postcolumn(post, thing->colormap, x, y1, y2, texmid, iscale, thing->scale);
        else
            draw_transpostcolumn(post, transtbls[thing->color], thing->colormap, x, y1, y2, texmid, iscale, thing->scale);
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

    float dist;
    float worldx, worldy;
    float dx, dy, step;
    float adjust;
    fixed_t wxf, wyf, dxf, dyf;
    int s, t;
    angle_t a1;
    int lightindex;
    uint8_t *map;

    dist = plane->z / ANGTAN(render_ytoangle(y));

    lightindex = dist / 16.0;
    lightindex = CLAMP(lightindex, 0, ZBANDS - 1);
    map = zmap[plane->baselight][lightindex];

    a1 = viewangle + xtoangle[x1];
    adjust = 1.0 / ANGCOS(a1 - viewangle);
    worldx = viewx + ANGCOS(a1) * dist * adjust;
    worldy = viewy + ANGSIN(a1) * dist * adjust;

    step = dist / projectconst;
    dx = ANGSIN(viewangle) * step;
    dy = -ANGCOS(viewangle) * step;

    worldx = fmodf(worldx, (float) FLAT_RES);
    worldy = fmodf(worldy, (float) FLAT_RES);

    wxf = FLOATTOFIXED(worldx);
    wyf = FLOATTOFIXED(worldy);
    dxf = FLOATTOFIXED(dx);
    dyf = FLOATTOFIXED(dy);

    for(x=x1; x<=x2; x++, wxf+=dxf, wyf+=dyf)
    {
        s = (wxf >> FIXEDSHIFT) & (FLAT_RES - 1);
        t = (wyf >> FIXEDSHIFT) & (FLAT_RES - 1);
        drawscreen->pixels[y * screenwidth + x] = map[((uint8_t*) plane->flat->cache)[t * FLAT_RES + s]];
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

    for(x=plane->x1; x<=plane->x2; x++)
    {
        if((plane->tops[x] == -1 && plane->bottoms[x] == -1) || plane->tops[x] > plane->bottoms[x])
            continue;
        
        tstep = 200.0 / screenheight;
        // tstep *= ANGCOS(xtoangle[x]);

        top = CLAMP(plane->tops[x], 0, screens[SCR_LVL].h - 1);
        bottom = CLAMP(plane->bottoms[x], 0, screens[SCR_LVL].h - 1);

        a = xtoangle[x] + viewangle;
        s = (float) a / (float) ANG90 * (float) SKYW;

        col = tex_getcolumn(levelskytex, s);
        draw_texcolumn(col, colormap->maps[0], x, top, bottom, 100 << FIXEDSHIFT, FLOATTOFIXED(tstep), FLOATTOFIXED(1.0 / tstep));
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

static fixed_t render_calcscale(angle_t angle, angle_t normal, fixed_t dist)
{
    fixed_t num, den, scale;

    num = fixedmul(FLOATTOFIXED(ANGCOS(angle - normal)), projectfrac);
    den = fixedmul(dist, FLOATTOFIXED(ANGCOS(angle - viewangle)));

    if (den > num >> FIXEDSHIFT)
    {
        scale = fixeddiv(num, den);

        if (scale > 64 << FIXEDSHIFT)
            scale = 64 << FIXEDSHIFT;
        else if (scale < 256)
            scale = 256;
    }
    else // division overflows
	    scale = 64 << FIXEDSHIFT;
	
    return scale;
}

void render_segrange(int x1, int x2, seg_t* seg)
{
    int x, y;

    fixed_t portaltop, portalbottom, worldtop, worldbottom;
    fixed_t dist, scale, scale1, scale2, scalestep, iscale;
    fixed_t top, topstep, midheight, midhstep;
    fixed_t tsilheight, portalheight, bsilheight, tsilstep, bsilstep;
    fixed_t toptexmid, midtexmid, bottexmid;
    int pltop, plbot;
    float sbase, s;
    angle_t normal, a1, a2, a;
    int pxtop, pxbottom;
    fixed_t ftop, fbottom;
    int mods, modt;
    bool drawceil, drawfloor, drawtop, drawbottom, drewtop, drewbottom;
    visplane_t *floorplane, *ceilplane;
    drawseg_t *drawseg, *maskedseg;
    uint8_t *column;
    int sectorlight, baselight, lightindex;
    uint8_t *map;
    
    a1 = xtoangle[x1] + viewangle;
    a2 = xtoangle[x2] + viewangle;

    normal = seg->angle + ANG90;
    // perpendicular distance from line to camera
    dist = fixedmag(FLOATTOFIXED(seg->v1->x) - viewxfrac, FLOATTOFIXED(seg->v1->y) - viewyfrac);
    dist = fixedmul(dist, FLOATTOFIXED(ANGCOS(unclippeda1 - normal)));

    scale = scale1 = render_calcscale(a1, normal, dist);
    if(x2 > x1)
    {
        scale2 = render_calcscale(a2, normal, dist);
        scalestep = (scale2 - scale1) / (x2 - x1);
    }
    else
    {
        scale2 = scale1;
        scalestep = 0;
    }

    worldtop = portaltop = FLOATTOFIXED(seg->frontside->sector->ceilheight);
    worldbottom = portalbottom = FLOATTOFIXED(seg->frontside->sector->floorheight);

    if(seg->backside && seg->backside->sector)
    {
        portaltop = MIN(portaltop, FLOATTOFIXED(seg->backside->sector->ceilheight));
        portalbottom = MAX(portalbottom, FLOATTOFIXED(seg->backside->sector->floorheight));
    }

    if(seg->frontside->sector->ceiltex == skylump && seg->backside && seg->backside->sector && seg->backside->sector->ceiltex == skylump)
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

    top = -fixedmul(worldtop - viewzfrac, scale1) + FLOATTOFIXED(halfy);
    topstep = -fixedmul(worldtop - viewzfrac, scalestep);

    midheight = fixedmul(portaltop - portalbottom, scale1);
    midhstep = fixedmul(portaltop - portalbottom, scalestep);

    tsilheight = bsilheight = 0;
    if(drawtop)
    {
        tsilheight = fixedmul(worldtop - portaltop, scale1);
        tsilstep = fixedmul(worldtop - portaltop, scalestep);

        if(seg->line->flags & LINEDEF_UPPERUNPEG)
            toptexmid = 0;
        else
            toptexmid = (seg->frontside->upper->h << FIXEDSHIFT) - (worldtop - portaltop);
        toptexmid += worldtop - viewzfrac + FLOATTOFIXED(seg->frontside->yoffs);
    }
    if(drawbottom)
    {
        bsilheight = fixedmul(portalbottom - worldbottom, scale1);
        bsilstep = fixedmul(portalbottom - worldbottom, scalestep);

        if(seg->line->flags & LINEDEF_LOWERUNPEG)
            bottexmid = worldtop - portalbottom;
        else
            bottexmid = 0;
        bottexmid += portalbottom - viewzfrac + FLOATTOFIXED(seg->frontside->yoffs);
    }

    if(seg->line->flags & LINEDEF_LOWERUNPEG)
        midtexmid = (seg->frontside->mid->h << FIXEDSHIFT) - (portaltop - portalbottom);
    else
        midtexmid = 0;
    midtexmid += portaltop - viewzfrac + FLOATTOFIXED(seg->frontside->yoffs);

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
    for(x=x1; x<=x2; x++, top+=topstep, midheight+=midhstep, scale+=scalestep)
    {
        iscale = 0xFFFFFFFFu / (uint32_t) scale + 1;

        a = xtoangle[x];
        s = sbase + FIXEDTOFLOAT(dist) * (ANGTAN(unclippeda1 - normal) - ANGTAN(a + viewangle - normal));
        
        lightindex = CLAMP(FIXEDTOFLOAT(scale) * 16 * 320.0 / screenwidth, 0, SCALEBANDS - 1);
        map = scalemap[baselight][lightindex];

        if(ceilplane)
        {
            pltop = topclips[x] + 1;
            plbot = top >> FIXEDSHIFT;

            if(plbot >= bottomclips[x])
                plbot = bottomclips[x] - 1;

            if(pltop <= plbot)
            {
                ceilplane->tops[x] = CLAMP(pltop, 0, screens[SCR_LVL].h - 1);
                ceilplane->bottoms[x] = CLAMP(plbot, 0, screens[SCR_LVL].h - 1);
            }
            else
                ceilplane->tops[x] = ceilplane->bottoms[x] = -1;
        }

        if(floorplane)
        {
            pltop = ((top + tsilheight + midheight + bsilheight) >> FIXEDSHIFT) + 1;
            plbot = bottomclips[x] - 1;

            if(pltop <= topclips[x])
                pltop = topclips[x] + 1;

            if(pltop <= plbot)
            {
                floorplane->tops[x] = CLAMP(pltop, 0, screens[SCR_LVL].h - 1);
                floorplane->bottoms[x] = CLAMP(plbot, 0, screens[SCR_LVL].h - 1);
            }
            else
                floorplane->tops[x] = floorplane->bottoms[x] = -1;
        }

        if(portaltop > portalbottom)
        {   
            ftop = top + tsilheight;
            fbottom = ftop + midheight;

            pxtop = (ftop >> FIXEDSHIFT) + 1;
            pxbottom = fbottom >> FIXEDSHIFT;

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
                topclips[x] = screens[SCR_LVL].h;
                bottomclips[x] = -1;

                if(pxtop <= pxbottom && seg->frontside->mid)
                {
                    column = tex_getcolumn(seg->frontside->mid, s);
                    draw_texcolumn(column, map, x, pxtop, pxbottom, midtexmid, iscale, scale);
                }
            }
            else if(pxtop <= pxbottom && seg->frontside->mid)
                maskedseg->maskeds[x - x1] = s;
        }

        if(drawtop)
        {
            ftop = top;
            fbottom = top + tsilheight;

            pxtop = (ftop >> FIXEDSHIFT) + 1;
            pxbottom = fbottom >> FIXEDSHIFT;

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
                column = tex_getcolumn(seg->frontside->upper, s);
                draw_texcolumn(column, map, x, pxtop, pxbottom, toptexmid, iscale, scale);
            }
        }

        if(drawbottom)
        {
            ftop = top + tsilheight + midheight;
            fbottom = ftop + bsilheight;

            pxtop = (ftop >> FIXEDSHIFT) + 1;
            pxbottom = fbottom >> FIXEDSHIFT;

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
                    column = tex_getcolumn(seg->frontside->lower, s);
                    draw_texcolumn(column, map, x, pxtop, pxbottom, bottexmid, iscale, scale);
                }
            }
        }

        if(drawtop)
            tsilheight += tsilstep;
        if(drawbottom)
            bsilheight += bsilstep;
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

    if(a1 < ANG360-HFOV/2 && a1 > HFOV/2)
        a1 = HFOV/2;
    if(a2 > HFOV/2 && a2 < ANG360-HFOV/2)
        a2 = ANG360-HFOV/2;
    
    x1 = angletox[(a1 + ANG90) >> TABSHIFT];
    x2 = angletox[(a2 + ANG90) >> TABSHIFT] - 1; // so we don't double up where segs meet

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
    fixed_t dx, dy, dz, dist, centerx, centery, xpos, ypos;
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
    if(dist < 4 << FIXEDSHIFT)
        return true;

    visthing->scale = fixeddiv(projectfrac, dist);

    xpos = fixedmul(dx, sinview) - fixedmul(dy, cosview);
    ypos = dz;

    centerx = FLOATTOFIXED(halfx) + fixedmul(xpos, visthing->scale);
    centery = FLOATTOFIXED(halfy) - fixedmul(ypos, visthing->scale);

    if(abs(xpos)>fixedmul(dist, FLOATTOFIXED(HPLANE)))
	    return true;

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

    if(x >= screenwidth || y >= screens[SCR_LVL].h)
        return true;

    w = fixedmul((fixed_t) pic->w << FIXEDSHIFT, visthing->scale) >> FIXEDSHIFT;
    h = fixedmul((fixed_t) pic->h << FIXEDSHIFT, visthing->scale) >> FIXEDSHIFT;

    if(x + w < 0 || y + h < 0)
        return true;

    visthing->x = x;
    visthing->y = y;

    visthing->color = mobj->info.color;

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
        bottomclips[i] = screens[SCR_LVL].h;
    }

    nvisplanes = 0;

    nvisthings = 0;
    visthinghead = NULL;

    ndrawsegs = 0;
    clipend = clipbuff;

    viewxfrac = FLOATTOFIXED(viewx);
    viewyfrac = FLOATTOFIXED(viewy);
    viewzfrac = FLOATTOFIXED(viewz);
}

void render(float progtime)
{
    drawscreen = &screens[SCR_LVL];
    render_setup();
    render_node(nnodes-1);   
    render_drawplanes();
    render_drawthings();
    visweapon_draw(progtime);

    frameindex++;
}