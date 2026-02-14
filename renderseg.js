import { pixels, gamewidth, gameheight } from './screen.js'
import { palettes, textures } from './wad.js';
import { player, curmap } from './main.js';
import { hfov, vfov } from './render.js';

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

function getsegsidedef(seg)
{
    const linedef = curmap.linedefs[seg.line];
    const sidedef = curmap.sidedefs[seg.isback ? linedef.backside : linedef.frontside];
    return sidedef;
}

export function renderseg(seg)
{
    const camz = player.camh + player.z;

    const v1 = curmap.vertices[seg.v1];
    const v2 = curmap.vertices[seg.v2];

    let rel1x = (v1.x - player.x) * Math.cos(-player.rot) - (v1.y - player.y) * Math.sin(-player.rot);
    let rel1y = (v1.x - player.x) * Math.sin(-player.rot) + (v1.y - player.y) * Math.cos(-player.rot);
    let rel2x = (v2.x - player.x) * Math.cos(-player.rot) - (v2.y - player.y) * Math.sin(-player.rot);
    let rel2y = (v2.x - player.x) * Math.sin(-player.rot) + (v2.y - player.y) * Math.cos(-player.rot);

    let a1 = Math.atan2(rel1y, rel1x);
    let a2 = Math.atan2(rel2y, rel2x);

    if(a2 >= a1)
        return;

    let x1 = angletopixel(a1);
    let x2 = angletopixel(a2);

    if(x1 >= x2)
        return;

    a1 = pixeltoangle(x1) + player.rot;
    a2 = pixeltoangle(x2) + player.rot;

    let i1 = rayline
    (
        player.x, player.y,
        Math.cos(a1), Math.sin(a1),
        v1.x, v1.y, v2.x, v2.y
    );
    
    let i2 = rayline
    (
        player.x, player.y,
        Math.cos(a2), Math.sin(a2),
        v1.x, v1.y, v2.x, v2.y
    );

    const height = 128;

    let segdirx = v2.x - v1.x;
    let segdiry = v2.y - v1.y;
    const seglen = Math.sqrt(segdirx * segdirx + segdiry * segdiry);
    segdirx /= seglen;
    segdiry /= seglen;

    const s1 = (i1.x - v1.x) * segdirx + (i1.y - v1.y) * segdiry;
    const s2 = (i2.x - v1.x) * segdirx + (i2.y - v1.y) * segdiry;
    const ttop = 0;
    const tbottom = height;

    let dist1 = Math.cos(player.rot) * (i1.x - player.x) + Math.sin(player.rot) * (i1.y - player.y);
    let dist2 = Math.cos(player.rot) * (i2.x - player.x) + Math.sin(player.rot) * (i2.y - player.y);

    let viewheight = 2 * Math.tan(vfov / 360 * Math.PI);
    let pixelheight = gameheight / viewheight;

    const height1 = height * pixelheight / dist1;
    const height2 = height * pixelheight / dist2;

    const y1top = -((height - camz) * pixelheight / dist1) + gameheight / 2;
    const y1bottom = y1top + height1;
    const y2top = -((height - camz) * pixelheight / dist2) + gameheight / 2;
    const y2bottom = y2top + height2;
    
    const side = getsegsidedef(seg);
    if(side.middle == '-')
        return;
    const tex = textures.get(side.middle);

    x1 = Math.floor(x1);
    x2 = Math.floor(x2);
    for(let x=Math.floor(x1); x<=Math.floor(x2); x++)
    {
        let alpha = (x - x1) / (x2 - x1);
        if(x1 == x2)
            alpha = 0;
        let top = (y2top - y1top) * alpha + y1top;
        let bottom = (y2bottom - y1bottom) * alpha + y1bottom;
        
        const tstep = (height) / (bottom - top);

        console.log(seg.height);

        const u = (alpha * dist1) / ((1 - alpha) * dist2 + alpha * dist1);
        const s = Math.floor(s1 + u * (s2 - s1));

        let samples = s;
        while(samples < 0)
            samples += tex.w;
        samples %= tex.w;

        let pxtop = Math.floor(top);
        let pxbottom = Math.floor(bottom);
        if(pxtop < 0) pxtop = 0;
        if(pxtop >= gameheight) pxtop = gameheight - 1;
        if(pxbottom < 0) pxbottom = 0;
        if(pxbottom >= gameheight) pxbottom = gameheight - 1;

        if(pxtop >= pxbottom)
            continue;

        let t = ttop + tstep * (pxtop - top);
        for(let y=pxtop; y<=pxbottom; y++, t+=tstep)
        {
            const v = (y - top) / (bottom - top);

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