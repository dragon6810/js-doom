#include "render.h"

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

float viewx = 0, viewy = 0, viewz = 0;
angle_t viewangle = 0;

int frameindex = 0;

typedef struct
{
    int16_t x1, x2;
} clipspan_t;

typedef struct visthing_s
{
    lumpinfo_t *patch;
    float scale;
    int x1, x2;
    int y; // bottom y-coordinate
    int height;
    float s1, sstep, t1;

    struct visthing_s *next, *prev;
} visthing_t;

typedef struct
{
    int16_t *bottom, *top; // both NULL, solid wall. one null, top or bottom wall. neither null, masked texture.
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
} visplane_t;

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
const int pastelcolors[npastelcolors] = { 16, 51, 83, 115, 161, 171, 194, 211, 226, 250 };

void render_init(void)
{
    int i;

    topclips = malloc(screenwidth * sizeof(int16_t));
    bottomclips = malloc(screenwidth * sizeof(int16_t));

    for(i=0; i<MAX_VISPLANE; i++)
    {
        visplanes[i].tops = malloc(screenwidth * sizeof(int16_t));
        visplanes[i].bottoms = malloc(screenwidth * sizeof(int16_t));
    }

    spanstarts = malloc(screenheight * sizeof(int16_t));

    clipbuffsize = 64 * screenwidth;
    clipbuff = malloc(clipbuffsize * sizeof(int16_t));

    visthingtop = malloc(screenwidth * sizeof(int16_t));
    visthingbottom = malloc(screenwidth * sizeof(int16_t));
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

    float alpha = (float)(screenheight / 2 - y) / (float)(screenheight / 2);
    float tangent = alpha * halfplane;
    
    return ANGATAN(tangent);
}

void render_clipthing(visthing_t* visthing)
{
    int x;
    drawseg_t *drawseg;

    float farthest, closest, scale;
    int x1, x2;

    for(drawseg=&drawsegs[ndrawsegs-1]; drawseg>=drawsegs; drawseg--)
    {
        farthest = MIN(drawseg->scale1, drawseg->scale2);
        closest = MAX(drawseg->scale1, drawseg->scale2);

        if(closest <= visthing->scale)
            continue;

        x1 = MAX(drawseg->x1, visthing->x1);
        x2 = MIN(drawseg->x2, visthing->x2);

        if(x1 > x2)
            continue;

        scale = drawseg->scale1 + (x1 - drawseg->x1) * drawseg->scalestep;
        for(x=x1; x<=x2; x++, scale+=drawseg->scalestep)
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
    int x, y;

    float s, t, invtstep, tmax;
    int posttop;
    pic_t *pic;
    post_t *post;
    int color;

    for(x=thing->x1; x<=thing->x2; x++)
    {
        visthingtop[x] = MAX(thing->y, 0);
        visthingbottom[x] = MIN(thing->y + thing->height, screenheight - 1);
    }

    render_clipthing(thing);

    pic = thing->patch->cache;

    invtstep = 1.0 / thing->sstep;
    tmax = thing->height * invtstep;

    s = thing->s1;
    for(x=thing->x1; x<=thing->x2; x++, s+=thing->sstep)
    {
        if(s >= pic->w)
            break;
        if(visthingtop[x] > visthingbottom[x])
            continue;

        post = ((uint8_t*) pic) + pic->postoffs[(int) s];
        while(post->ystart != 0xFF)
        {
            posttop = thing->y + post->ystart * invtstep;
            for(y=posttop, t=0; t<post->len; t+=thing->sstep, y++)
            {
                if(y < visthingtop[x])
                    continue;
                if(y > visthingbottom[x])
                    break;

                color = post->payload[(int)t];
                pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
            }
            post = (post_t*) (((uint8_t*) post) + sizeof(post_t) + post->len + 1);
        }
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

    for(i = 0; i < nvisthings; i++)
    {
        best = unsorted.next;

        for(it = unsorted.next; it != &unsorted; it = it->next)
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
    visthing_t *visthing;

    render_sortthings();
    for(visthing=visthingtail; visthing; visthing=visthing->prev)
        render_drawthing(visthing);
}

void render_drawspan(visplane_t* plane, int y, int x1, int x2)
{
    // how many pixels wide is a unit when it is 1 unit from the camera
    const float pixelwidth = (float) screenwidth / (float) HPLANE;

    int x;

    int color;
    float dist;
    float worldx, worldy;
    float dx, dy, step;
    float adjust;
    int s, t;
    angle_t a1;

    dist = plane->z / ANGTAN(render_ytoangle(y));

    worldx = viewx;
    worldy = viewy;

    a1 = viewangle + render_xtoangle(x1);
    adjust = 1.0 / ANGCOS(a1 - viewangle);
    worldx += ANGCOS(a1) * dist * adjust;
    worldy += ANGSIN(a1) * dist * adjust;

    step = dist / pixelwidth;

    dx = ANGSIN(viewangle) * step;
    dy = -ANGCOS(viewangle) * step;

    for(x=x1; x<=x2; x++, worldx+=dx, worldy+=dy)
    {
        s = (int) worldx & (FLAT_RES - 1);
        t = (int) worldy & (FLAT_RES - 1);
        color = ((uint8_t*) plane->flat->cache)[t * FLAT_RES + s];
        pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
    }
}

void render_drawplane(visplane_t* plane)
{
    int x, y;

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
        return plane;
    }

    if(x2 < plane->x1 || x1 > plane->x2)
    {
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
        return &visplanes[nvisplanes++];
    }

    plane->x1 = MIN(x1, plane->x1);
    plane->x2 = MAX(x2, plane->x2);
    return plane;
}

visplane_t* render_getvisplane(float z, lumpinfo_t* flat)
{
    int i;

    for(i=nvisplanes-1; i>=0; i--)
        if(visplanes[i].z == z && visplanes[i].flat == flat)
            return &visplanes[i];

    if(nvisplanes >= MAX_VISPLANE)
    {
        fprintf(stderr, "render_splitplane: MAX_VISPLANE reached\n");
        return NULL;
    }
    
    visplanes[nvisplanes].x1 = visplanes[nvisplanes].x2 = -1;
    visplanes[nvisplanes].z = z;
    visplanes[nvisplanes].flat = flat;
    return &visplanes[nvisplanes++];
}

void render_segrange(int x1, int x2, seg_t* seg)
{
    // at dist = 1 from camera, how many pixels wide/tall is 1 unit?
    const float unitpixels = screenwidth / HPLANE;

    int x, y;

    int color;
    int dx;
    float portaltop, portalbottom, worldtop, worldbottom;
    float dist, scale, scale1, scale2, scalestep;
    float top1, top2, topstep, h1, h2, hstep;
    float top, height;
    float tsilheight, portalheight, bsilheight;
    int ctop, cbot;
    float sbase, s;
    angle_t normal, a1, a2, a;
    int pxtop, pxbottom;
    float ftop, fbottom;
    float ttop, t, tstep;
    int mods, modt;
    int16_t topclip, bottomclip;
    bool drawceil, drawfloor, drawtop, drawbottom, drewtop, drewbottom;
    visplane_t *floorplane, *ceilplane;
    int pxmax, pxmin;
    drawseg_t *drawseg;
    
    a1 = render_xtoangle(x1) + viewangle;
    a2 = render_xtoangle(x2) + viewangle;

    normal = seg->angle + ANG90;
    // perpendicular distance from line to camera
    dist = magnitude(seg->v1->x - viewx, seg->v1->y - viewy) * ANGCOS(unclippeda1 - normal);

    scale1 = ANGCOS(a1 - normal) / (dist * ANGCOS(a1 - viewangle));
    scale2 = ANGCOS(a2 - normal) / (dist * ANGCOS(a2 - viewangle));

    dx = x2 - x1;
    scalestep = (scale2 - scale1) / (float) dx;

    worldtop = portaltop = seg->frontside->sector->ceilheight;
    worldbottom = portalbottom = seg->frontside->sector->floorheight;

    if(seg->backside && seg->backside->sector)
    {
        portaltop = MIN(portaltop, seg->backside->sector->ceilheight);
        portalbottom = MAX(portalbottom, seg->backside->sector->floorheight);
    }

    drawtop = worldtop > portaltop;
    drawbottom = worldbottom < portalbottom;

    if(seg->frontside->mid)
        tex_stitch(seg->frontside->mid);
    if(drawtop && seg->frontside->upper)
        tex_stitch(seg->frontside->upper);
    if(drawbottom && seg->frontside->lower)
        tex_stitch(seg->frontside->lower);

    top1 = -(worldtop - viewz) * scale1 * unitpixels + screenheight / 2;
    top2 = -(worldtop - viewz) * scale2 * unitpixels + screenheight / 2;
    topstep = (top2 - top1) / (float) dx;

    h1 = (worldtop - worldbottom) * scale1 * unitpixels;
    h2 = (worldtop - worldbottom) * scale2 * unitpixels;
    hstep = (h2 - h1) / (float) dx;

    drawceil = true;
    if(seg->frontside->sector->ceilheight <= viewz)
        drawceil = false;
    if(seg->backside 
    && seg->backside->sector->ceilheight == seg->frontside->sector->ceilheight
    && seg->backside->sector->ceiltex == seg->frontside->sector->ceiltex
    && seg->backside->sector->floorheight < seg->backside->sector->ceilheight)
        drawceil = false;

    drawfloor = true;
    if(seg->frontside->sector->floorheight >= viewz)
        drawfloor = false;
    if(seg->backside 
    && seg->backside->sector->floorheight == seg->frontside->sector->floorheight
    && seg->backside->sector->floortex == seg->frontside->sector->floortex
    && seg->backside->sector->ceilheight > seg->backside->sector->floorheight)
        drawfloor = false;
    
    floorplane = ceilplane = NULL;
    if(drawceil)
    {
        ceilplane = render_getvisplane(seg->frontside->sector->ceilheight - viewz, seg->frontside->sector->ceiltex);
        ceilplane = render_splitplane(ceilplane, x1, x2);
    }
    if(drawfloor)
    {
        floorplane = render_getvisplane(seg->frontside->sector->floorheight - viewz, seg->frontside->sector->floortex);
        floorplane = render_splitplane(floorplane, x1, x2);
    }

    sbase = seg->frontside->xoffs + seg->offset;

    drewtop = drewbottom = false;
    for(x=x1, scale=scale1, top=top1, height=h1; x<=x2; x++, top+=topstep, height+=hstep, scale+=scalestep)
    {
        topclip = topclips[x];
        bottomclip = bottomclips[x];

        tstep = 1.0 / (unitpixels * scale);

        a = render_xtoangle(x);
        s = sbase + dist * (ANGTAN(unclippeda1 - normal) - ANGTAN(a + viewangle - normal));

        pxmax = -1;
        pxmin = screenheight;

        tsilheight = (worldtop - portaltop) * unitpixels * scale;
        portalheight = (portaltop - portalbottom) * unitpixels * scale;
        bsilheight = (portalbottom - worldbottom) * unitpixels * scale;

        // middle
        if(portaltop > portalbottom)
        {   
            ftop = top + tsilheight;
            fbottom = top + tsilheight + portalheight;

            pxtop = ceilf(ftop);
            pxbottom = fbottom;

            pxtop = MAX(pxtop, topclips[x] + 1);
            pxbottom = MIN(pxbottom, bottomclips[x] - 1);
            
            if(pxtop <= pxbottom && pxtop < bottomclips[x] && pxbottom > topclips[x])
            {
                topclip = pxtop - 1;
                bottomclip = pxbottom + 1;

                pxmax = MAX(pxmax, pxbottom);
                pxmin = MIN(pxmin, pxtop);

                if(!seg->backside && seg->frontside->mid)
                {
                    if(seg->line->flags & LINEDEF_LOWERUNPEG)
                        ttop = seg->frontside->mid->h - (portaltop - portalbottom) + seg->frontside->yoffs;
                    else
                        ttop = seg->frontside->yoffs;

                    t = tstep * ((float) pxtop + 0.5 - ftop) + ttop;
                    while(t < 0)
                        t += seg->frontside->mid->h;

                    mods = ((int) s % seg->frontside->mid->w + seg->frontside->mid->w) % seg->frontside->mid->w;
                    for(y=pxtop; y<=pxbottom; y++, t+=tstep)
                    {
                        modt = (int) t % seg->frontside->mid->h;
                        color = seg->frontside->mid->stitch[mods * seg->frontside->mid->h + modt];
                        pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
                    }
                }
            }
        }

        if(drawtop)
        {
            ftop = top;
            fbottom = top + tsilheight;

            pxtop = ceilf(ftop);
            pxbottom = fbottom;

            pxtop = MAX(pxtop, topclips[x] + 1);
            pxbottom = MIN(pxbottom, bottomclips[x] - 1);

            if(pxtop <= pxbottom && pxtop < bottomclips[x] && pxbottom > topclips[x])
            {
                pxmax = MAX(pxmax, pxbottom);
                pxmin = MIN(pxmin, pxtop);

                if(seg->frontside->upper)
                {
                    if(seg->line->flags & LINEDEF_UPPERUNPEG)
                        ttop = seg->frontside->yoffs;
                    else
                        ttop = seg->frontside->upper->h - (worldtop - portaltop) + seg->frontside->yoffs;

                    t = tstep * ((float) pxtop + 0.5 - ftop) + ttop;
                    while(t < 0)
                        t += seg->frontside->upper->h;

                    mods = ((int) s % seg->frontside->upper->w + seg->frontside->upper->w) % seg->frontside->upper->w;

                    for(y=pxtop; y<=pxbottom; y++, t+=tstep)
                    {
                        modt = (int) t % seg->frontside->upper->h;
                        color = seg->frontside->upper->stitch[mods * seg->frontside->upper->h + modt];   
                        pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
                    }
                }
            }
        }

        if(drawbottom)
        {
            ftop = top + tsilheight + portalheight;
            fbottom = top + height;

            pxtop = ceilf(ftop);
            pxbottom = fbottom;

            pxtop = MAX(pxtop, topclips[x] + 1);
            pxbottom = MIN(pxbottom, bottomclips[x] - 1);

            if(pxtop <= pxbottom && pxtop < bottomclips[x] && pxbottom > topclips[x])
            {
                pxmax = MAX(pxmax, pxbottom);
                pxmin = MIN(pxmin, pxtop);

                if(seg->frontside->lower)
                {
                    if(seg->line->flags & LINEDEF_LOWERUNPEG)
                        ttop = worldtop - portalbottom + seg->frontside->yoffs;
                    else
                        ttop = seg->frontside->yoffs;

                    t = tstep * ((float) pxtop + 0.5 - ftop) + ttop;
                    while(t < 0)
                        t += seg->frontside->lower->h;

                    mods = ((int) s % seg->frontside->lower->w + seg->frontside->lower->w) % seg->frontside->lower->w;
                    for(y=pxtop; y<=pxbottom; y++, t+=tstep)
                    {
                        modt = (int) t % seg->frontside->lower->h;
                        color = seg->frontside->lower->stitch[mods * seg->frontside->lower->h + modt];
                        pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
                    }
                }
            }
        }

        if(ceilplane && pxmin != screenheight)
        {
            ctop = topclips[x] + 1;
            cbot = pxmin - 1;

            ceilplane->tops[x] = CLAMP(ctop, 0, screenheight - 1);
            ceilplane->bottoms[x] = CLAMP(cbot, 0, screenheight - 1);
            if(ctop > cbot || cbot < 0 || ctop >= screenheight)
                ceilplane->tops[x] = ceilplane->bottoms[x] = -1;
        }
        if(floorplane && pxmax != -1)
        {
            ctop = pxmax + 1;
            cbot = bottomclips[x] - 1;

            floorplane->tops[x] = CLAMP(ctop, 0, screenheight - 1);
            floorplane->bottoms[x] = CLAMP(cbot, 0, screenheight - 1);

            if(ctop > cbot || cbot < 0 || ctop >= screenheight)
                floorplane->tops[x] = floorplane->bottoms[x] = -1;
        }

        if(drawceil || drawtop)
        {
            topclips[x] = topclip;
            drewtop = seg->backside != NULL;
        }
        if(drawfloor || drawbottom)
        {
            bottomclips[x] = bottomclip;
            drewbottom = seg->backside != NULL;
        }
    }

    if(!seg->backside || seg->backside->sector->floorheight == seg->backside->sector->ceilheight)
    {
        if(ndrawsegs >= MAX_DRAWSEG)
            return;

        drawseg = &drawsegs[ndrawsegs++];

        drawseg->x1 = x1;
        drawseg->x2 = x2;
        drawseg->scale1 = scale1;
        drawseg->scale2 = scale2;
        drawseg->scalestep = scalestep;
        drawseg->top = drawseg->bottom = NULL;
    }

    if(drewtop)
    {
        if(ndrawsegs >= MAX_DRAWSEG)
            return;
        if(clipend + (x2 + 1 - x1) >= clipbuff + clipbuffsize)
            return;

        drawseg = &drawsegs[ndrawsegs++];

        drawseg->x1 = x1;
        drawseg->x2 = x2;
        drawseg->scale1 = scale1;
        drawseg->scale2 = scale2;
        drawseg->scalestep = scalestep;
        drawseg->bottom = NULL;
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

        drawseg->x1 = x1;
        drawseg->x2 = x2;
        drawseg->scale1 = scale1;
        drawseg->scale2 = scale2;
        drawseg->scalestep = scalestep;
        drawseg->top = NULL;
        drawseg->bottom = clipend;
        clipend += x2 + 1 - x1;

        memcpy(drawseg->bottom, bottomclips + x1, (x2 + 1 - x1) * sizeof(int16_t));
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
    if(theta > ANG180)
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
    if(seg->frontside->sector->ceilheight == seg->backside->sector->ceilheight
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
    // at dist = 1 from camera, how many pixels wide/tall is 1 unit?
    const float unitpixels = screenwidth / HPLANE;

    float dx, dy, dist, centerx, centery;
    float w, h;
    int x1, x2, top;
    pic_t *pic;

    dx = mobj->x - viewx;
    dy = mobj->y - viewy;

    dist = dx * ANGCOS(viewangle) + dy * ANGSIN(viewangle);
    if(dist < 0.05)
        return true;

    visthing->scale = 1.0 / dist;
    
    centerx = (dx * ANGSIN(viewangle) + dy * -ANGCOS(viewangle)) * visthing->scale * unitpixels + screenwidth / 2;
    centery = -(mobj->z - viewz) * visthing->scale * unitpixels + screenheight / 2;

    visthing->patch = sprites[states[mobj->state].sprite][states[mobj->state].frame & 0x7FFF];
    if(!visthing->patch)
        return true;
    wad_cache(visthing->patch);
    pic = visthing->patch->cache;

    w = (float) pic->w * visthing->scale * unitpixels;
    h = (float) pic->h * visthing->scale * unitpixels;

    x1 = centerx - pic->xoffs * visthing->scale * unitpixels;
    x2 = x1 + w;
    top = centery - pic->yoffs * visthing->scale * unitpixels;

    if(x2 < 0|| x1 >= screenwidth)
        return true;
    if(top >= screenheight || top + h < 0)
        return true;

    visthing->sstep = dist / unitpixels;

    visthing->x1 = MAX(x1, 0);
    visthing->x2 = MIN(x2, screenwidth - 1);
    visthing->y = top;
    visthing->height = h;
    visthing->s1 = (float) (visthing->x1 - x1) * visthing->sstep;
    visthing->t1 = (float) (visthing->y - top) * visthing->sstep;

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
    for(i=0; i<MAX_VISPLANE; i++)
    {
        for(x=0; x<screenwidth; x++)
            visplanes[i].bottoms[x] = visplanes[i].tops[x] = -1;
    }

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