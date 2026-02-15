import { setpixel } from "./renderseg.js";
import { gamewidth } from "./screen.js";
import { flats } from "./wad.js";

export class Visplane
{
    constructor(minx, maxx, height, tex)
    {
        this.minx = minx;
        this.maxx = maxx;
        this.height = height;
        this.tex = tex;

        this.tops = [];
        this.tops.length = gamewidth;
        this.tops.fill(-1);
        this.bottoms = [];
        this.bottoms.length = gamewidth;
        this.bottoms.fill(-1);
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

function drawplane(visplane)
{
    const flat = flats.get(visplane.tex);

    if(flat == null)
        return;

    for(let x=visplane.minx; x<=visplane.maxx; x++)
    {
        for(let y=visplane.tops[x]; y<=visplane.bottoms[x]; y++)
        {
            setpixel(x, y, sampleflat(flat, x, y));
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