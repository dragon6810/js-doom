#include "render.h"

#include "level.h"
#include "screen.h"
#include "tex.h"
#include "wad.h"

#include <stdlib.h>
#include <string.h>

#define MAX_CLIPSPAN 192
#define MAX_VISPLANE 256
#define FLAT_RES 64

float viewx = 0, viewy = 0, viewz = 0;
angle_t viewangle = 0;

typedef struct
{
    int16_t x1, x2;
} clipspan_t;

typedef struct
{
    int16_t x1, x2;
    int16_t *tops;
    int16_t *bottoms;
    float z; // relative to viewz
    lumpinfo_t *flat;
} visplane_t;

typedef struct
{
    int x1, x2;
    float scale1, scale2;
    int16_t *topclip, *bottomclip;
    lumpinfo_t *masktex;
} drawseg_t;

int nclipspans = 0;
clipspan_t clipspans[MAX_CLIPSPAN];
int16_t *topclips = NULL;
int16_t *bottomclips = NULL;
int16_t *spanstarts = NULL;

int nvisplanes = 0;
visplane_t visplanes[MAX_VISPLANE];

angle_t unclippeda1;

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

void render_drawspan(visplane_t* plane, int y, int x1, int x2)
{
    // how many pixels wide is a unit when it is 1 unit from the camera
    const float pixelwidth = (float) screenwidth / (float) HPLANE;

    int x;

    int color;
    float dist;
    float worldx, worldy;
    float dx, dy, step;
    int s, t;

#if 0
    if(y < 0 || y >= screenheight || x1 < 0 || x2 < 0 || x1 >= screenwidth || x2 >= screenwidth)
    {
        fprintf(stderr, "render_drawspan: out of bounds span y=%d (%d, %d)\n", y, x1, x2);
        return;
    }
#endif

    dist = plane->z / ANGTAN(render_ytoangle(y));

    worldx = viewx;
    worldy = viewy;

    worldx += ANGCOS(viewangle) * dist;
    worldy += ANGSIN(viewangle) * dist;

    step = dist / pixelwidth;

    dx = ANGSIN(viewangle) * step;
    dy = -ANGCOS(viewangle) * step;

    worldx += dx * (float) (x1 - screenwidth / 2);
    worldy += dy * (float) (x1 - screenwidth / 2);

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
    // how many pixels tall is a unit when it is 1 unit from the camera
    const float pixelheight = (float) screenheight / (float) VPLANE;

    int x, y;

    int color;
    int dx;
    float portaltop, portalbottom, worldtop, worldbottom;
    float dist, scale, scale1, scale2, scalestep;
    float top1, top2, topstep, h1, h2, hstep;
    float top, height, tsil, bsil, tsil1, tsil2, bsil1, bsil2, tsilstep, bsilstep;
    int ctop, cbot;
    float sbase, s;
    angle_t normal, a1, a2, a;
    int pxtop, pxbottom;
    float t, tstep;
    int mods, modt;
    int16_t topclip, bottomclip;
    bool drawceil, drawfloor;
    visplane_t *floorplane, *ceilplane;
    int pxmax, pxmin;

    color = pastelcolors[(seg - segs) % npastelcolors];
    
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

    if(seg->frontside->mid)
        tex_stitch(seg->frontside->mid);
    if(seg->frontside->upper)
        tex_stitch(seg->frontside->upper);
    if(seg->frontside->lower)
        tex_stitch(seg->frontside->lower);

    top1 = -(portaltop - viewz) * scale1 * pixelheight + screenheight / 2;
    top2 = -(portaltop - viewz) * scale2 * pixelheight + screenheight / 2;
    topstep = (top2 - top1) / (float) dx;

    h1 = (portaltop - portalbottom) * scale1 * pixelheight;
    h2 = (portaltop - portalbottom) * scale2 * pixelheight;
    hstep = (h2 - h1) / (float) dx;
    tsil1 = (worldtop - portaltop) * scale1 * pixelheight;
    tsil2 = (worldtop - portaltop) * scale2 * pixelheight;
    tsilstep = (tsil2 - tsil1) / (float) dx;
    bsil1 = (portalbottom - worldbottom) * scale1 * pixelheight;
    bsil2 = (portalbottom - worldbottom) * scale2 * pixelheight;
    bsilstep = (bsil2 - bsil1) / (float) dx;

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

    for(x=x1, scale=scale1, top=top1, height=h1, tsil=tsil1, bsil=bsil1; x<=x2; x++, top+=topstep, height+=hstep, scale+=scalestep, tsil+=tsilstep, bsil+=bsilstep)
    {
        topclip = topclips[x];
        bottomclip = bottomclips[x];

        tstep = 1.0 / (pixelheight * scale);

        a = render_xtoangle(x);
        s = sbase + dist * (ANGTAN(unclippeda1 - normal) - ANGTAN(a + viewangle - normal));

        t = tstep * (pxtop - top);
        color = (int) s % 256;

        pxmin = pxmax = -1;

        // middle
        pxtop = top;
        pxbottom = top + height;

        topclip = MAX(pxtop - 1, topclips[x]);
        bottomclip = MIN(pxbottom + 1, bottomclips[x]);

        if(pxtop <= pxbottom)
        {
            pxmin = MIN(pxtop, bottomclips[x]);
            pxmax = MAX(pxbottom, topclips[x]);
        }

        pxtop = MAX(pxtop, topclips[x] + 1);
        pxbottom = MIN(pxbottom, bottomclips[x] - 1);
        
        if(seg->frontside->mid && pxtop <= pxbottom)
        {
            if (seg->line->flags & LINEDEF_LOWERUNPEG)
                t = tstep * (pxtop - (top + height)) + seg->frontside->mid->h + seg->frontside->yoffs;
            else
                t = tstep * (pxtop - top) + seg->frontside->yoffs;
            mods = ((int) s % seg->frontside->mid->w + seg->frontside->mid->w) % seg->frontside->mid->w;
            for(y=pxtop; y<=pxbottom; y++, t+=tstep)
            {
                color = (int) (s + t) % 256;
                modt = ((int) t % seg->frontside->mid->h + seg->frontside->mid->h) % seg->frontside->mid->h;
                color = seg->frontside->mid->stitch[mods * seg->frontside->mid->h + modt];
                
                pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
            }
        }

        if(worldtop - portaltop > 0)
        {
            pxtop = top - tsil;
            pxbottom = top - 1;

            pxtop = MAX(pxtop, topclips[x] + 1);
            pxbottom = MIN(pxbottom, bottomclips[x] - 1);

            if(pxtop < bottomclips[x] && pxbottom > topclips[x])
            {
                pxmin = pxtop;

                if(seg->frontside->upper && pxtop <= pxbottom)
                {
                    t = tstep * (pxtop - top) + seg->frontside->yoffs;
                    if(!(seg->line->flags & LINEDEF_UPPERUNPEG))
                        t += seg->frontside->upper->h - (worldtop - portaltop);
                    mods = ((int) s % seg->frontside->upper->w + seg->frontside->upper->w) % seg->frontside->upper->w;
                    for(y=pxtop; y<=pxbottom; y++, t+=tstep)
                    {
                        color = (int) (s + t) % 256;
                        modt = ((int) t % seg->frontside->upper->h + seg->frontside->upper->h) % seg->frontside->upper->h;
                        color = seg->frontside->upper->stitch[mods * seg->frontside->upper->h + modt];
                        
                        pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
                    }
                }
            }
        }

        if(portalbottom - worldbottom > 0)
        {
            pxtop = top + height + 1;
            pxbottom = top + height + 1 + bsil;

            pxtop = MAX(pxtop, topclips[x] + 1);
            pxbottom = MIN(pxbottom, bottomclips[x] - 1);

            if(pxtop < bottomclips[x] && pxbottom > topclips[x])
            {
                pxmax = pxbottom;

                if(seg->frontside->lower && pxtop <= pxbottom)
                {
                    t = tstep * (pxtop - top);
                    if(seg->line->flags & LINEDEF_LOWERUNPEG)
                        t -= (portaltop - worldbottom);
                    else
                        t -= (portaltop - portalbottom);
                    mods = ((int) s % seg->frontside->lower->w + seg->frontside->lower->w) % seg->frontside->lower->w;
                    for(y=pxtop; y<=pxbottom; y++, t+=tstep)
                    {
                        color = (int) (s + t) % 256;
                        modt = ((int) t % seg->frontside->lower->h + seg->frontside->lower->h) % seg->frontside->lower->h;
                        color = seg->frontside->lower->stitch[mods * seg->frontside->lower->h + modt];
                        
                        pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
                    }
                }
            }
        }

        if(ceilplane && pxmin != -1)
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

        if(drawceil || worldtop - portaltop > 0)
            topclips[x] = topclip;
        if(drawfloor || portalbottom - worldbottom > 0)
            bottomclips[x] = bottomclip;
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

void render_subsector(int issector)
{
    int i;

    ssector_t *ssector;

    ssector = &ssectors[issector];
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
}

void render(void)
{
    render_setup();
    render_node(nnodes-1);   
    render_drawplanes();
}