#include "render.h"

#include "level.h"
#include "screen.h"
#include "tex.h"
#include "wad.h"

#define MAX_CLIPSPAN 192

float viewx = 0, viewy = 0, viewz = 0;
angle_t viewangle = 0;

typedef struct
{
    int16_t x1, x2;
} clipspan_t;

int nclipspans = 0;
clipspan_t clipspans[MAX_CLIPSPAN];

angle_t unclippeda1;

const int npastelcolors = 10;
const int pastelcolors[npastelcolors] = { 16, 51, 83, 115, 161, 171, 194, 211, 226, 250 };

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
    if(x >= screenwidth)
        x = screenwidth - 1;
    
    return x;
}

angle_t render_xtoangle(int x)
{
    const float halfplane = HPLANE / 2;

    float alpha, tangent;

    alpha = 1.0 - (2.0 * x) / (float) screenwidth;
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
    float worldtop, worldbottom;
    float dist, dist1, dist2, scale, scale1, scale2, scalestep;
    float top1, top2, topstep, h1, h2, hstep;
    float top, height;
    angle_t normal, a1, a2;

    color = pastelcolors[(seg - segs) % npastelcolors];

    a1 = render_xtoangle(x1) + viewangle;
    a2 = render_xtoangle(x2) + viewangle;

    normal = seg->angle + ANG90;
    // perpendicular distance from line to camera
    dist = magnitude(seg->v1->x - viewx, seg->v1->y - viewy) * ANGCOS(unclippeda1 - normal);

    dist1 = dist / ANGCOS(a1 - normal) * ANGCOS(a1 - viewangle);
    dist2 = dist / ANGCOS(a2 - normal) * ANGCOS(a2 - viewangle);

    scale1 = 1.0 / dist1;
    scale2 = 1.0 / dist2;
    dx = x2 - x1;
    scalestep = (scale2 - scale1) / (float) dx;

    worldtop = seg->frontside->sector->ceilheight;
    if(seg->backside && seg->backside->sector->ceilheight < worldtop)
        worldtop = seg->backside->sector->ceilheight;
    worldbottom = seg->frontside->sector->floorheight;
    if(seg->backside && seg->backside->sector->floorheight > worldbottom)
        worldbottom = seg->backside->sector->floorheight;

    top1 = -(worldtop - viewz) * scale1 * pixelheight + screenheight / 2;
    top2 = -(worldtop - viewz) * scale2 * pixelheight + screenheight / 2;
    topstep = (top2 - top1) / (float) dx;

    h1 = (worldtop - worldbottom) * scale1 * pixelheight;
    h2 = (worldtop - worldbottom) * scale2 * pixelheight;
    hstep = (h2 - h1) / (float) dx;

    for(x=x1, top=top1, height=h1; x<=x2; x++, top+=topstep, height+=hstep)
    {
        for(y=top; y<top+height; y++)
            pixels[y * screenwidth + x] = (int) palette[color].r << 16 | (int) palette[color].g << 8 | (int) palette[color].b;
    }
}

void render_clipandaddseg(int x1, int x2, seg_t *seg)
{
    int i, j;

    if (x1 > x2)
        return;
    
    i = 0;
    while(clipspans[i].x2 < x1 - 1)
        i++;

    if(x2<clipspans[i].x1 - 1)
    {
        render_segrange(x1, x2, seg);

        if(nclipspans >= MAX_CLIPSPAN)
        {
            fprintf(stderr, "render_clipandaddseg: max clipspans reached\n");
            return;
        }

        for(j=nclipspans-1; j>=i; j--)
            clipspans[j + 1] = clipspans[j];
        nclipspans++;

        clipspans[i].x1 = x1;
        clipspans[i].x2 = x2;

        if(i && clipspans[i - 1].x2 >= clipspans[i].x1 - 1)
        {
            if(clipspans[i].x2 > clipspans[i - 1].x2)
                clipspans[i - 1].x2 = clipspans[i].x2;

            for(j=i; j<nclipspans-1; j++)
                clipspans[j] = clipspans[j + 1];
            nclipspans--;
            i--;
        }

        while(i + 1 < nclipspans && clipspans[i].x2 >= clipspans[i + 1].x1 - 1)
        {
            if(clipspans[i + 1].x2 > clipspans[i].x2)
                clipspans[i].x2 = clipspans[i + 1].x2;

            for(j = i + 1; j < nclipspans - 1; j++)
                clipspans[j] = clipspans[j + 1];
            nclipspans--;
        }

        return;
    }

    if(x1 < clipspans[i].x1)
        render_segrange(x1, clipspans[i].x1 - 1, seg);

    if(x1 < clipspans[i].x1)
        clipspans[i].x1 = x1;
    if(x2 > clipspans[i].x2)
        clipspans[i].x2 = x2;

    if(i && clipspans[i - 1].x2 >= clipspans[i].x1 - 1)
    {
        if(clipspans[i].x2 > clipspans[i - 1].x2)
            clipspans[i - 1].x2 = clipspans[i].x2;

        for(j=i; j<nclipspans-1; j++)
            clipspans[j] = clipspans[j + 1];
        nclipspans--;
        i--;
    }

    while(i + 1 < nclipspans && clipspans[i].x2 >= clipspans[i + 1].x1 - 1)
    {
        if(clipspans[i + 1].x2 > clipspans[i].x2)
            clipspans[i].x2 = clipspans[i + 1].x2;

        for(j=i+1; j<nclipspans-1; j++)
            clipspans[j] = clipspans[j + 1];
        nclipspans--;
    }

    if(x2 > clipspans[i].x2)
        render_segrange(clipspans[i].x2 + 1, x2, seg);
}

void render_clipseg(int x1, int x2, seg_t* seg)
{
    int i;

    i = 0;
    while(clipspans[i].x2 < x1)
        i++;
    
    for(; x2>=clipspans[i].x1 && x1<=x2; i++)
    {
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

    if(a1 < -(HFOV/2) && a1 > (HFOV/2) && a1 - (HFOV/2) >= theta)
        return;
    if(a2 > (HFOV/2) && a2 < -(HFOV/2) && -(HFOV/2) - a2 >= theta)
        return;

    if(a1 < ANG180 && a1 > HFOV/2)
        a1 = HFOV/2;
    if(a2 > ANG180 && a2 < -HFOV/2)
        a2 = -HFOV/2;
    
    x1 = render_angletox(a1);
    x2 = render_angletox(a2) - 1; // so we don't double up where segs meet

    if(x1 > x2)
        return;

    render_segrange(x1, x2, seg);

    /*
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
    */
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

    render_node(node->children[!side]);
    render_node(node->children[side]);
}

void render_setup(void)
{
    nclipspans = 2;
    clipspans[0].x1 = INT16_MIN;
    clipspans[0].x2 = -1;
    clipspans[1].x1 = screenwidth;
    clipspans[1].x2 = INT16_MAX;
}

void render(void)
{
    render_node(nnodes-1);   
}