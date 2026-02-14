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
export var topclips = [];
export var bottomclips = [];

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

function getsegsidedef(seg, back)
{
    const flip = seg.isback ^ back;

    const linedef = curmap.linedefs[seg.line];
    const index = flip ? linedef.backside : linedef.frontside;
    return index == -1 ? null : curmap.sidedefs[index];
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

function clamp(x, min, max)
{
    if(x < min)
        x = min;
    if(x > max)
        x = max;
    return x;
}

function sampletex(tex, x, y)
{
    x = Math.floor(x);
    y = Math.floor(y);

    x = (x % tex.w + tex.w) % tex.w;
    y = (y % tex.h + tex.h) % tex.h;
    return tex.graphic.data[y * tex.w + x];
}

function drawfragment(x1, x2, linedef, frontside, backside)
{
    const viewheight = 2 * Math.tan(vfov / 360 * Math.PI);
    const pixelheight = gameheight / viewheight;

    const v1 = curmap.vertices[linedef.v1];
    const v2 = curmap.vertices[linedef.v2];

    const frontsector = frontside ? curmap.sectors[frontside.sector] : null;
    const backsector = backside ? curmap.sectors[backside.sector] : null;

    const midtop = (backsector ? Math.min(frontsector.ceil, backsector.ceil) : frontsector.ceil) - viewz;
    const midbottom = (backsector ? Math.max(frontsector.floor, backsector.floor) : frontsector.floor) - viewz;
    const maxtop = frontsector.ceil - viewz;
    const maxbottom = frontsector.floor - viewz;

    const drawceilstep = backsector != null && backsector.ceil < frontsector.ceil;
    const drawfloorstep = backsector != null && backsector.floor > frontsector.floor;

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

    const dist1 = Math.cos(player.rot) * (i1.x - viewx) + Math.sin(player.rot) * (i1.y - viewy);
    const dist2 = Math.cos(player.rot) * (i2.x - viewx) + Math.sin(player.rot) * (i2.y - viewy);
    const scale1 = 1 / dist1;
    const scale2 = 1 / dist2;

    const midheight1 = (midtop - midbottom) * pixelheight * scale1;
    const midheight2 = (midtop - midbottom) * pixelheight * scale2;
    const ceilheight1 = backsector == null ? null : (frontsector.ceil - backsector.ceil) * pixelheight * scale1;
    const ceilheight2 = backsector == null ? null : (frontsector.ceil - backsector.ceil) * pixelheight * scale2;
    const floorheight1 = backsector == null ? null : (backsector.floor - frontsector.floor) * pixelheight * scale1;
    const floorheight2 = backsector == null ? null : (backsector.floor - frontsector.floor) * pixelheight * scale2;

    const y1top = -(midtop * pixelheight / dist1) + gameheight / 2;
    const y1bottom = y1top + midheight1;
    const y2top = -(midtop * pixelheight / dist2) + gameheight / 2;
    const y2bottom = y2top + midheight2;
    
    const toptex = textures.get(frontside.upper);
    const midtex = textures.get(frontside.middle);
    const bottomtex = textures.get(frontside.lower);

    for(let x=x1; x<=x2; x++)
    {
        const topclip = topclips[x];
        const bottomclip = bottomclips[x]; 

        const alpha = (x - x1) / (x2 - x1);
        const baseytop = (y2top - y1top) * alpha + y1top;
        const baseybottom = (y2bottom - y1bottom) * alpha + y1bottom;
        
        const curscale = alpha * scale2 + (1 - alpha) * scale1;

        const tstep = 1 / (pixelheight * curscale);
        const u = (alpha * scale2) / ((1 - alpha) * scale1 + alpha * scale2);

        const s = Math.floor(s1 + u * (s2 - s1)) - frontside.xoffs;

        let fulltop = gameheight;
        let fullbottom = -1;

        // ceiling step
        if(drawceilstep)
        {
            const stepheight = alpha * (ceilheight2 - ceilheight1) + ceilheight1;
            const ytop = baseytop - stepheight;
            const ybottom = baseytop;
            const pxtop = Math.floor(clamp(ytop, topclip, bottomclip - 1));
            const pxbottom = Math.floor(clamp(ybottom, topclip, bottomclip - 1));

            if(toptex != null && pxtop < pxbottom)
            {
                let t = tstep * (pxtop - ytop) - frontside.yoffs;
                for(let y=pxtop; y<=pxbottom; y++, t+=tstep)
                    setpixel(x, y, sampletex(toptex, s, t));
            }

            fulltop = Math.min(fulltop, pxtop);
            fullbottom = Math.max(fullbottom, pxbottom);
        }

        // middle
        {
            const pxtop = Math.floor(clamp(baseytop, topclip, bottomclip - 1));
            const pxbottom = Math.floor(clamp(baseybottom, topclip, bottomclip - 1));

            if(midtex != null && pxtop < pxbottom)
            {
                let t = tstep * (pxtop - baseytop) - frontside.yoffs;
                for(let y=pxtop; y<=pxbottom; y++, t+=tstep)
                    setpixel(x, y, sampletex(midtex, s, t));
            }

            fulltop = Math.min(fulltop, pxtop);
            fullbottom = Math.max(fullbottom, pxbottom);

            bottomclips[x] = pxbottom;
            topclips[x] = pxtop + 1;
        }

        // floor step
        if(drawfloorstep)
        {
            const stepheight = alpha * (floorheight2 - floorheight1) + floorheight1;
            const ytop = baseybottom;
            const ybottom = baseybottom + stepheight;
            const pxtop = Math.floor(clamp(ytop, topclip, bottomclip - 1));
            const pxbottom = Math.floor(clamp(ybottom, topclip, bottomclip - 1));

            if(bottomtex != null && pxtop < pxbottom)
            {
                let t = tstep * (pxtop - ytop) - frontside.yoffs;
                for(let y=pxtop; y<=pxbottom; y++, t+=tstep)
                    setpixel(x, y, sampletex(bottomtex, s, t));
            }

            fulltop = Math.min(fulltop, pxtop);
            fullbottom = Math.max(fullbottom, pxbottom);
        }
    }
}

function cliprange(x1, x2, linedef, frontside, backside, issolid)
{
    let drawspans = [];

    let i = 0;
    let curx = x1;
    for(i=0; i<wallspans.length && curx<x2; i++)
    {
        const curspan = wallspans[i];
        
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
    
    if(issolid && drawspans.length > 0)
    {
        wallspans.push(...structuredClone(drawspans));
        wallspans.sort((a, b) => a.x1 - b.x1);
        mergespans();
    }
        
    for(i=0; i<drawspans.length; i++)
        drawfragment(drawspans[i].x1, drawspans[i].x2, linedef, frontside, backside);
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

    const frontside = getsegsidedef(seg, false);
    const backside = getsegsidedef(seg, true);
    const frontsector = frontside ? curmap.sectors[frontside.sector] : null;
    const backsector = backside ? curmap.sectors[backside.sector] : null;
    const line = curmap.linedefs[seg.line];

    let issolid = false;

    if(backsector == null)
        issolid = true;
    else if(frontside.middle != '-')
        issolid = true;
    else if(backsector.ceil <= backsector.floor)
        issolid = true;
    else
    {
        if(frontsector.floor == backsector.floor
        && frontsector.ceil == backsector.ceil
        && frontsector.floortex == backsector.floortex
        && frontsector.ceiltex  == backsector.ceiltex)
            return;
    }

    cliprange(x1, x2, line, frontside, backside, issolid);
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
    topclips.length = gamewidth;
    topclips.fill(0);
    bottomclips.length = gamewidth;
    bottomclips.fill(gameheight);
}