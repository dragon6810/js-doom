import { player } from "./main.js";
import { hfov, vfov, viewx, viewy } from "./render.js";
import { setpixel } from "./renderseg.js";
import { gameheight, gamewidth } from "./screen.js";
import { flats } from "./wad.js";

export class Visplane
{
    constructor(minx, maxx, height, tex)
    {
        this.minx = minx;
        this.maxx = maxx;
        this.height = height;
        this.tex = tex;

        this.tops = new Int16Array(gamewidth).fill(-1);
        this.bottoms = new Int16Array(gamewidth).fill(-1);
    }
};

export var visplanes = [];

function sampleflat(flat, x, y)
{
    x = Math.floor(x);
    y = Math.floor(y);

    x = (x % flat.w + flat.w) % flat.w;
    y = (y % flat.h + flat.h) % flat.h;
    return flat.data[y * flat.w + x];
}

function yslope(y)
{
    y = 2 * (-y / gameheight + 0.5);
    y *= Math.tan(vfov / 360 * Math.PI);
    return y;
}

function drawplane(visplane)
{
    const half = hfov / 360 * Math.PI;
    const viewwidth = 2 * Math.tan(half);              // plane width at z=1
    const dplanex = viewwidth / gamewidth;             // plane-x units per pixel
    const planeX0 = -viewwidth / 2;                    // left edge plane-x

    const flat = flats.get(visplane.tex);
    if (flat == null) return;

    const fwdx = Math.cos(player.rot);
    const fwdy = Math.sin(player.rot);
    const rightx = Math.sin(player.rot);               // right at rot=0 => (0,-1)
    const righty = -Math.cos(player.rot);

    for (let y=visplane.miny; y<=visplane.maxy; y++)
    {
        const tanv = yslope(y);

        const dist = visplane.height / tanv;

        const stepmag = dist * dplanex;
        const xstep = rightx * stepmag;
        const ystep = righty * stepmag;

        // base at x = visplane.minx
        const planeX = planeX0 + visplane.minx * dplanex;

        let s = viewx + dist * (fwdx + rightx * planeX);
        let t = viewy + dist * (fwdy + righty * planeX);

        for (let x=visplane.minx; x<=visplane.maxx; x++, s+=xstep, t+=ystep)
        {
            if (y < visplane.tops[x]) continue;
            if (y > visplane.bottoms[x]) continue;
            setpixel(x, y, sampleflat(flat, s, t));
        }
    }
}

export function renderplanes()
{
    for(let i=0; i<visplanes.length; i++)
        drawplane(visplanes[i]);
}

export function addvisplane(visplane)
{
    // TODO: merging?
    visplanes.push(visplane);
}

export function clearplanes()
{
    visplanes.length = 0;
}