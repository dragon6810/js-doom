import { pixels, gamewidth, gameheight } from './screen.js'
import { palettes, textures } from './wad.js';
import { player, curmap } from './main.js';
import { hfov, vfov, viewx, viewy, viewz } from './render.js';

export class Wallspan
{
    constructor(x1, x2)
    {
        this.x1 = x1;
        this.x2 = x2;
    }
}

export var wallspans = [];

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

    return Math.floor(pixel);
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

    const qpx = ax - px;
    const qpy = ay - py;
  
    const t = cross2(qpx, qpy, sx, sy) / rxs;
    const u = cross2(qpx, qpy, dx, dy) / rxs;
  
    return { x: px + t * dx, y: py + t * dy, t, u };
}

function getsegsidedef(seg)
{
    const linedef = curmap.linedefs[seg.line];
    const sidedef = curmap.sidedefs[seg.isback ? linedef.backside : linedef.frontside];
    return sidedef;
}

function mergespans()
{
    for(let i=0; i<wallspans.length - 1;) 
    {
        const a = wallspans[i];
        const b = wallspans[i + 1];
    
        if(b.x1 == a.x2 + 1)
        {
            a.x2 = b.x2;
            wallspans.splice(i + 1, 1);
            continue;
        } 
        
        i++;
    }
}

function drawline(x1, x2, bottom, top, linedef, sidedef)
{
    const v1 = curmap.vertices[linedef.v1];
    const v2 = curmap.vertices[linedef.v2];

    const a1 = pixeltoangle(x1) + player.rot;
    const a2 = pixeltoangle(x2) + player.rot;

    let i1 = rayline
    (
        viewx, viewy,
        Math.cos(a1), Math.sin(a1),
        v1.x, v1.y, v2.x, v2.y
    );
    
    let i2 = rayline
    (
        viewx, viewy,
        Math.cos(a2), Math.sin(a2),
        v1.x, v1.y, v2.x, v2.y
    );

    let segdirx = v2.x - v1.x;
    let segdiry = v2.y - v1.y;
    const seglen = Math.sqrt(segdirx * segdirx + segdiry * segdiry);
    segdirx /= seglen;
    segdiry /= seglen;

    const s1 = (i1.x - v1.x) * segdirx + (i1.y - v1.y) * segdiry;
    const s2 = (i2.x - v1.x) * segdirx + (i2.y - v1.y) * segdiry;
    const ttop = 0;

    let dist1 = Math.cos(player.rot) * (i1.x - viewx) + Math.sin(player.rot) * (i1.y - viewy);
    let dist2 = Math.cos(player.rot) * (i2.x - viewx) + Math.sin(player.rot) * (i2.y - viewy);

    const viewheight = 2 * Math.tan(vfov / 360 * Math.PI);
    const pixelheight = gameheight / viewheight;

    const height1 = (top - bottom) * pixelheight / dist1;
    const height2 = (top - bottom) * pixelheight / dist2;

    const y1top = -(top * pixelheight / dist1) + gameheight / 2;
    const y1bottom = y1top + height1;
    const y2top = -(top * pixelheight / dist2) + gameheight / 2;
    const y2bottom = y2top + height2;
    
    if(sidedef.middle == '-')
        return;
    const tex = textures.get(sidedef.middle);

    for(let x=x1; x<=x2; x++)
    {
        let alpha = (x - x1) / (x2 - x1);
        let ytop = (y2top - y1top) * alpha + y1top;
        let ybottom = (y2bottom - y1bottom) * alpha + y1bottom;
        
        const tstep = (top - bottom) / (ybottom - ytop);

        const u = (alpha * dist1) / ((1 - alpha) * dist2 + alpha * dist1);
        const s = Math.floor(s1 + u * (s2 - s1));

        let samples = s;
        while(samples < 0)
            samples += tex.w;
        samples %= tex.w;

        let pxtop = Math.floor(ytop);
        let pxbottom = Math.floor(ybottom);
        if(pxtop < 0) pxtop = 0;
        if(pxtop >= gameheight) pxtop = gameheight - 1;
        if(pxbottom < 0) pxbottom = 0;
        if(pxbottom >= gameheight) pxbottom = gameheight - 1;

        if(pxtop >= pxbottom)
            continue;

        let t = ttop + tstep * (pxtop - ytop);
        for(let y=pxtop; y<=pxbottom; y++, t+=tstep)
        {
            let tsample = Math.floor(t);
            while(tsample < 0)
                tsample += tex.h;
            tsample %= tex.h;

            // console.log(t);
            setpixel(x, y, tex.graphic.data[tsample * tex.w + samples]);
            //setpixel(x, y, 224);
        }
    }
}

function cliprange(x1, x2, seg)
{
    /*
    const sidedef = getsegsidedef(seg);
        const sector = curmap.sectors[sidedef.sector];
        const line = curmap.linedefs[seg.line];

    drawline(x1, x2, sector.floor - viewz, sector.ceil - viewz, line, getsegsidedef(seg));
    return;*/
    let drawspans = [];

    let i = 0;
    let curx = x1;
    for(i=0; i<wallspans.length && curx<x2; i++)
    {
        let curspan = wallspans[i];
        
        // completely to the right of curspan
        if(curx > curspan.x2)
            continue;
        
        // completely to the left of curspan
        if(x2 < curspan.x1)
            break;

        // OVERLAPS:

        // fragment bit of span to the left of curspan
        if(curx < curspan.x1)
        {
            const span = new Wallspan(curx, curspan.x1 - 1);
            drawspans.push(span);
        }

        curx = curspan.x2 + 1;
    }

    if(curx<x2)
    {
        const span = new Wallspan(curx, x2);
        drawspans.push(span);
    }

    if(drawspans.length > 0)
    {
        wallspans.push(...drawspans);
        wallspans.sort((a, b) => a.x1 - b.x1);
        mergespans();
    }

    for(i=0; i<drawspans.length; i++)
    {
        const sidedef = getsegsidedef(seg);
        const sector = curmap.sectors[sidedef.sector];
        const line = curmap.linedefs[seg.line];

        drawline(drawspans[i].x1, drawspans[i].x2, sector.floor - viewz, sector.ceil - viewz, line, getsegsidedef(seg));
    }
}

function normalizeangle(a)
{
    while (a <= -Math.PI)
        a += 2 * Math.PI;
    while (a > Math.PI)
        a -= 2 * Math.PI;
    return a;
}

export function renderseg(seg)
{
    const v1 = curmap.vertices[seg.v1];
    const v2 = curmap.vertices[seg.v2];

    let a1 = normalizeangle(Math.atan2(v1.y - viewy, v1.x - viewx) - player.rot);
    let a2 = normalizeangle(Math.atan2(v2.y - viewy, v2.x - viewx) - player.rot);

    while (a2 > a1)
        a2 -= 2 * Math.PI;
    if(a1 - a2 > Math.PI)
        return;

    if(a1 > hfov / 360 * Math.PI && a2 > hfov / 360 * Math.PI)
        return;
    if(a1 < -hfov / 360 * Math.PI && a2 < -hfov / 360 * Math.PI)
        return;

    let x1 = angletopixel(a1);
    let x2 = angletopixel(a2);

    if(x1 >= x2)
        return;

    const sidedef = getsegsidedef(seg);
    if(sidedef.middle == '-')
        return;

    cliprange(x1, x2, seg);
}

function setpixel(x, y, palindex)
{
    if(palindex == 247)
        return;
    
    const r = palettes[0].data[palindex * 3 + 0];
    const g = palettes[0].data[palindex * 3 + 1];
    const b = palettes[0].data[palindex * 3 + 2];

    pixels[(y * gamewidth + x) * 4 + 0] = r;
    pixels[(y * gamewidth + x) * 4 + 1] = g;
    pixels[(y * gamewidth + x) * 4 + 2] = b;
}

export function drawfullseg(seg)
{
    renderseg(seg);
}

export function rendersegsclear()
{
    wallspans.length = 0;
}