import { pixels, gamewidth, gameheight, pixels32 } from './screen.js'
import { palettes, textures } from './wad.js';
import { player, curmap } from './main.js';
import { hfov, vfov, viewangle, viewx, viewy, viewz } from './render.js';
import { addvisplane, Visplane } from './renderplane.js';

export class Wallspan
{
    constructor(x1, x2)
    {
        this.x1 = x1;
        this.x2 = x2;
    }
}

export var wallspans = [];
export var topclips = new Uint16Array(gamewidth);
export var bottomclips = new Uint16Array(gamewidth);

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

function getscale(dist, a, normal)
{
    return Math.cos(a - normal) / (dist * Math.cos(a - viewangle));
}

var unclippeda1 = 0;

function drawfragment(x1, x2, seg, linedef, frontside, backside)
{
    const viewheight = 2 * Math.tan(vfov / 360 * Math.PI);
    const pixelheight = gameheight / viewheight;

    const v1 = curmap.vertices[seg.v1];
    const v2 = curmap.vertices[seg.v2];
    // this actually points around the back of the seg, but its useful for calculations.
    const normal = seg.angle + Math.PI / 2;

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

    const straightdist = Math.sqrt(Math.pow(v1.x - viewx, 2) + Math.pow(v1.y - viewy, 2))
    // perpendicular distance from line to player
    const dist = Math.cos(unclippeda1 - normal) * straightdist;
    const scale1 = getscale(dist, a1, normal);
    const scale2 = getscale(dist, a2, normal);
    const scalestep = x1 == x2 ? 0 : (scale2 - scale1) / (x2 - x1);

    const sbase = seg.offset + frontside.xoffs;

    const midheight1 = (midtop - midbottom) * pixelheight * scale1;
    const midheightstep = ((midtop - midbottom) * pixelheight * scale2 - midheight1) / (x2 - x1);
    const ceilheight1 = backsector == null ? null : (frontsector.ceil - backsector.ceil) * pixelheight * scale1;
    const ceilheightstep = backsector == null ? null : (((frontsector.ceil - backsector.ceil) * pixelheight * scale2) - ceilheight1) / (x2 - x1);
    const ceilheight2 = backsector == null ? null : (frontsector.ceil - backsector.ceil) * pixelheight * scale2;
    const floorheight1 = backsector == null ? null : (backsector.floor - frontsector.floor) * pixelheight * scale1;
    const floorheightstep = backsector == null ? null : (((backsector.floor - frontsector.floor) * pixelheight * scale2) - floorheight1) / (x2 - x1);

    const top1 = -(midtop * pixelheight * scale1) + gameheight / 2;
    const ytopstep = ((-(midtop * pixelheight * scale2) + gameheight / 2) - top1) / (x2 - x1);
    
    const toptex = textures.get(frontside.upper);
    const midtex = textures.get(frontside.middle);
    const bottomtex = textures.get(frontside.lower);

    let makeceiling = true;
    let makefloor = true;
    if(viewz >= frontsector.ceil)
        makeceiling = false;
    if(viewz <= frontsector.floor)
        makefloor = false;

    const ceilplane = makeceiling ? new Visplane(x1, x2, frontsector.ceil - viewz, frontsector.ceiltex) : null;
    const floorplane = makefloor ? new Visplane(x1, x2, frontsector.floor - viewz, frontsector.floortex) : null;

    let scale = scale1;
    let midheight = midheight1;
    let ceilheight = ceilheight1;
    let floorheight = floorheight1;
    let ytop = top1;
    for(let x=x1; x<=x2; x++, scale+=scalestep, midheight+=midheightstep, ceilheight+=ceilheightstep, floorheight+=floorheightstep, ytop+=ytopstep)
    {
        const topclip = topclips[x];
        const bottomclip = bottomclips[x]; 

        const worldangle = pixeltoangle(x) + viewangle;
        const b = worldangle - normal;
        const a = unclippeda1 - worldangle;
        const s = sbase + dist * (Math.tan(a + b) - Math.tan(b));

        const tstep = 1 / (pixelheight * scale);

        let fulltop = gameheight;
        let fullbottom = -1;

        // ceiling step
        if(drawceilstep)
        {
            const siltop = ytop - ceilheight;
            const silbottom = ytop - 1;
            const pxtop = Math.floor(clamp(siltop, topclip + 1, bottomclip - 1));
            const pxbottom = Math.floor(clamp(silbottom, topclip + 1, bottomclip - 1));

            let ttop = -(maxtop - midtop);
            if(linedef.upperunpegged)
                ttop = 0;

            if(toptex != null && pxtop < pxbottom)
            {
                let t = tstep * (pxtop - siltop) + frontside.yoffs + ttop;
                for(let y=pxtop; y<=pxbottom; y++, t+=tstep)
                    setpixel(x, y, sampletex(toptex, s, t));
            }

            fulltop = Math.min(fulltop, pxtop);
            fullbottom = Math.max(fullbottom, pxbottom);
        }

        // middle
        {
            const pxtop = Math.floor(clamp(ytop, topclip + 1, bottomclip - 1));
            const pxbottom = Math.floor(clamp(ytop+midheight, topclip + 1, bottomclip - 1));

            let ttop = 0;
            if(linedef.lowerunpegged)
                ttop = -(midtop - midbottom);

            if(midtex != null && pxtop < pxbottom)
            {
                let t = tstep * (pxtop - ytop) + frontside.yoffs + ttop;
                for(let y=pxtop; y<=pxbottom; y++, t+=tstep)
                    setpixel(x, y, sampletex(midtex, s, t));
            }

            fulltop = Math.min(fulltop, pxtop);
            fullbottom = Math.max(fullbottom, pxbottom);

            bottomclips[x] = pxbottom + 1;
            topclips[x] = pxtop - 1;
        }

        // floor step
        if(drawfloorstep)
        {
            const siltop = ytop+midheight+1;
            const silbottom = ytop+midheight+floorheight;
            const pxtop = Math.floor(clamp(siltop, topclip + 1, bottomclip - 1));
            const pxbottom = Math.floor(clamp(silbottom, topclip + 1, bottomclip - 1));

            let ttop = -(midbottom - maxbottom);
            if(linedef.lowerunpegged)
                ttop = -(maxtop - midbottom);

            if(bottomtex != null && pxtop < pxbottom)
            {
                let t = tstep * (pxtop - siltop) + frontside.yoffs + ttop;
                for(let y=pxtop; y<=pxbottom; y++, t+=tstep)
                    setpixel(x, y, sampletex(bottomtex, s, t));
            }

            fulltop = Math.min(fulltop, pxtop);
            fullbottom = Math.max(fullbottom, pxbottom);
        }

        if(makeceiling)
        {
            ceilplane.tops[x] = topclip + 1;
            ceilplane.bottoms[x] = fulltop;
        }
        if(makefloor)
        {
            floorplane.tops[x] = fullbottom;
            floorplane.bottoms[x] = bottomclip - 1;
        }
    }

    if(makeceiling)
        addvisplane(ceilplane);
    if(makefloor)
        addvisplane(floorplane);
}

function cliprange(x1, x2, seg, linedef, frontside, backside, issolid)
{
    let drawspans = [];

    let i = 0;
    let curx = x1;
    for(i=0; i<wallspans.length && curx<=x2; i++)
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

    if(curx<=x2)
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
        drawfragment(drawspans[i].x1, drawspans[i].x2, seg, linedef, frontside, backside);
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
    const halffov = hfov / 360 * Math.PI;

    const v1 = curmap.vertices[seg.v1];
    const v2 = curmap.vertices[seg.v2];

    let a1 = normalizeangle(Math.atan2(v1.y - viewy, v1.x - viewx) - player.rot);
    let a2 = normalizeangle(Math.atan2(v2.y - viewy, v2.x - viewx) - player.rot);

    if(a1 - a2 < -Math.PI)
        a1 += Math.PI * 2;
    if(a1 - a2 > Math.PI)
        a1 -= Math.PI * 2;

    if(a2 > a1)
        return;

    const theta = Math.min(Math.abs(a1 - a2), Math.PI * 2 - Math.abs(a1 - a2));

    if(a1 > halffov && a2 < -Math.PI / 2)
        return;
    if(a2 < -halffov && a1 > Math.PI / 2)
        return;

    if(a1 > halffov && a2 > halffov)
        return;
    if(a1 < -halffov && a2 < -halffov)
        return;

    let x1 = angletopixel(a1);
    let x2 = angletopixel(a2);

    if(x1 > x2)
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

    unclippeda1 = a1 + viewangle;

    cliprange(x1, x2, seg, line, frontside, backside, issolid);
}

export function setpixel(x, y, palindex)
{
    pixels32[y * gamewidth + x] = palettes[0].data[palindex];
}

export function drawfullseg(seg)
{
    renderseg(seg);
}

export function rendersegsclear()
{
    wallspans.length = 0;
    topclips.fill(0);
    bottomclips.fill(gameheight);
}