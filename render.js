import { player } from './main.js';
import { pixels, gamewidth, gameheight } from './screen.js'
import { segs } from './world.js'

class Visplane
{
    constructor(xstart, xstop)
    {
        this.xstart = xstart;
        this.xstop = xstop;
        this.bottoms = [];
        this.tops = [];
        this.bottoms.length = gamewidth;
        this.tops.length = gamewidth;
    }
};

const hfov = 90;
const vfov = hfov * gameheight / gamewidth;

var visplanes = [];

// a in radians
export function angletopixel(a)
{
    if(a < -hfov / 360 * Math.PI)
        a = -hfov / 360 * Math.PI;
    if(a > hfov / 360 * Math.PI)
        a = hfov / 360 * Math.PI;

    let halfwidth = Math.tan(hfov / 360 * Math.PI);
    let planeloc = Math.tan(a) / halfwidth / 2;
    let pixel = (-planeloc + 0.5) * gamewidth;
    if(pixel < 0)
        pixel = 0;
    if(pixel >= gamewidth)
        pixel = gamewidth - 1;

    return pixel;
}

// returns radians
export function pixeltoangle(pixel)
{
    let halfwidth = Math.tan(hfov / 360 * Math.PI);
    let planeloc = 0.5 - (pixel / gamewidth);
    let a = Math.atan(2 * halfwidth * planeloc);
    return a;
}

function cross2(ax, ay, bx, by)
{
    return ax * by - ay * bx;
}

function rayline(px, py, dx, dy, ax, ay, bx, by)
{
    const sx = bx - ax;
    const sy = by - ay;
  
    const rxs = cross2(dx, dy, sx, sy);
    if (Math.abs(rxs) < 1e-12)
    {
        return null;
    }
  
    const qpx = ax - px;
    const qpy = ay - py;
  
    const t = cross2(qpx, qpy, sx, sy) / rxs;
    const u = cross2(qpx, qpy, dx, dy) / rxs;
  
    return { x: px + t * dx, y: py + t * dy, t, u };
}

export function drawplane(plane)
{
    for(let x=plane.xstart; x<plane.xstop; x++)
    {
        for(let y=plane.tops[x]; y<plane.bottoms[x]; y++)
        {
            pixels[(y * gamewidth + x) * 4 + 0] = 0;
            pixels[(y * gamewidth + x) * 4 + 1] = 255;
            pixels[(y * gamewidth + x) * 4 + 2] = 0;
        }
    }
}

export function renderseg(seg)
{
    const camz = player.camh + player.z;

    let rel1x = (seg.x1 - player.x) * Math.cos(-player.rot) - (seg.y1 - player.y) * Math.sin(-player.rot);
    let rel1y = (seg.x1 - player.x) * Math.sin(-player.rot) + (seg.y1 - player.y) * Math.cos(-player.rot);
    let rel2x = (seg.x2 - player.x) * Math.cos(-player.rot) - (seg.y2 - player.y) * Math.sin(-player.rot);
    let rel2y = (seg.x2 - player.x) * Math.sin(-player.rot) + (seg.y2 - player.y) * Math.cos(-player.rot);

    let a1 = Math.atan2(rel1y, rel1x);
    let a2 = Math.atan2(rel2y, rel2x);

    if(a2 >= a1)
        return;

    let x1 = angletopixel(a1);
    let x2 = angletopixel(a2);

    if(x1 > x2)
        return;

    a1 = pixeltoangle(x1) + player.rot;
    a2 = pixeltoangle(x2) + player.rot;

    let i1 = rayline
    (
        player.x, player.y,
        Math.cos(a1), Math.sin(a1),
        seg.x1, seg.y1, seg.x2, seg.y2
    );
    
    let i2 = rayline
    (
        player.x, player.y,
        Math.cos(a2), Math.sin(a2),
        seg.x1, seg.y1, seg.x2, seg.y2
    );

    let dist1 = Math.cos(player.rot) * (i1.x - player.x) + Math.sin(player.rot) * (i1.y - player.y);
    let dist2 = Math.cos(player.rot) * (i2.x - player.x) + Math.sin(player.rot) * (i2.y - player.y);

    let viewheight = 2 * Math.tan(vfov / 360 * Math.PI);
    let pixelheight = gameheight / viewheight;

    let y1top = -((seg.top - camz) * pixelheight / dist1) + gameheight / 2;
    let y1bottom = -((seg.bottom - camz) * pixelheight / dist1) + gameheight / 2;
    let y2top = -((seg.top - camz) * pixelheight / dist2) + gameheight / 2;
    let y2bottom = -((seg.bottom - camz) * pixelheight / dist2) + gameheight / 2;

    let cieling = new Visplane(Math.floor(x1), Math.floor(x2 + 1));
    let floor = new Visplane(Math.floor(x1), Math.floor(x2 + 1));

    for(let x=Math.floor(x1); x<=Math.floor(x2); x++)
    {
        let t = (x - x1) / (x2 - x1);
        let top = Math.floor((y2top - y1top) * t + y1top);
        let bottom = Math.floor((y2bottom - y1bottom) * t + y1bottom);

        if(top < 0) top = 0;
        if(top >= gameheight) top = gameheight - 1;
        if(bottom < 0) bottom = 0;
        if(bottom >= gameheight) bottom = gameheight - 1;

        if(top >= bottom)
            continue;

        cieling.bottoms[x] = top;
        cieling.tops[x] = 0;
        floor.tops[x] = bottom+1;
        floor.bottoms[x] = gameheight;

        for(let y=top; y<=bottom; y++)
        {
            pixels[(y * gamewidth + x) * 4 + 0] = 0;
            pixels[(y * gamewidth + x) * 4 + 1] = 0;
            pixels[(y * gamewidth + x) * 4 + 2] = 255;
        }
    }

    visplanes.push(cieling);
    visplanes.push(floor);
}

export function render()
{
    visplanes.length = 0;

    for(let i=0; i<segs.length; i++)
        renderseg(segs[i]);

    for(let i=0; i<visplanes.length; i++)
        drawplane(visplanes[i]);
}