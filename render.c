#include "render.h"

#include "level.h"
#include "screen.h"
#include "tex.h"
#include "wad.h"

#include <stdlib.h>

#define MAX_CLIPSPAN 192

float viewx = 0, viewy = 0, viewz = 0;
angle_t viewangle = 0;

typedef struct
{
    int16_t x1, x2;
} clipspan_t;

int nclipspans = 0;
clipspan_t clipspans[MAX_CLIPSPAN];
int16_t *topclips = NULL;
int16_t *bottomclips = NULL;

angle_t unclippeda1;

const int npastelcolors = 10;
const int pastelcolors[npastelcolors] = { 16, 51, 83, 115, 161, 171, 194, 211, 226, 250 };

void render_init(void)
{
    topclips = malloc(screenwidth * sizeof(int16_t));
    bottomclips = malloc(screenwidth * sizeof(int16_t));
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
    float sbase, s;
    angle_t normal, a1, a2, a;
    int pxtop, pxbottom;
    float t, tstep;
    int mods, modt;
    int16_t topclip, bottomclip;

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

        if(pxtop < screenheight && pxbottom >= 0)
        {
            pxtop = top;
            pxbottom = top + height;

            pxtop = MAX(pxtop, topclips[x] + 1);
            pxbottom = MIN(pxbottom, bottomclips[x] - 1);

            topclip = pxtop - 1;
            bottomclip = pxbottom + 1;
            
            if(seg->frontside->mid)
            {
                t = tstep * (pxtop - top);
                if(seg->line->flags & LINEDEF_LOWERUNPEG)
                    t -= (worldtop - worldbottom);
                mods = ((int) s % seg->frontside->mid->w + seg->frontside->mid->w) % seg->frontside->mid->w;
                for(y=pxtop; y<=pxbottom; y++, t+=tstep)
                {
                    color = (int) (s + t) % 256;
                    modt = ((int) t % seg->frontside->mid->h + seg->frontside->mid->h) % seg->frontside->mid->h;
                    color = seg->frontside->mid->stitch[mods * seg->frontside->mid->h + modt];
                    
                    pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
                }
            }
        }

        if(top - tsil < screenheight && tsil >= 1)
        {
            pxtop = top - tsil;
            pxbottom = top - 1;

            pxtop = MAX(pxtop, topclips[x] + 1);
            pxbottom = MIN(pxbottom, bottomclips[x] - 1);

            if(seg->frontside->upper)
            {
                t = tstep * (pxtop - top);
                if(seg->line->flags & LINEDEF_UPPERUNPEG)
                    t += (worldtop - portaltop);
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

        if(top + height + bsil >= 0 && bsil >= 1)
        {
            pxtop = top + height + 1;
            pxbottom = top + height + 1 + bsil;

            pxtop = MAX(pxtop, topclips[x] + 1);
            pxbottom = MIN(pxbottom, bottomclips[x] - 1);

            if(seg->frontside->lower)
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

        topclips[x] = topclip;
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
    int i;

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
}

void render(void)
{
    render_setup();
    render_node(nnodes-1);   
}