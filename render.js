import { player } from './main.js';
import { pixels, gamewidth, gameheight } from './screen.js'
import { segs } from './world.js'

const hfov = 90;
const vfov = hfov * gameheight / gamewidth;

// a in radians
export function angletopixel(a)
{
    let halfwidth = Math.tan(hfov / 360 * Math.PI);
    let planeloc = Math.tan(a) / halfwidth / 2;
    let pixel = (-planeloc + 0.5) * gamewidth;
    if(pixel < 0)
        pixel = 0;
    if(pixel >= gamewidth)
        pixel = gamewidth - 1;

    return Math.floor(pixel);
}

export function renderseg(seg)
{
    let a1 = Math.atan2(seg.y1 - player.y, seg.x1 - player.x) - player.rot;
    let a2 = Math.atan2(seg.y2 - player.y, seg.x2 - player.x) - player.rot;

    console.log("angles: {}, {}", a1, a2);

    if(a2 >= a1)
        return;

    if(a1 - a2 > Math.PI)
        return;

    let x1 = angletopixel(a1);
    let x2 = angletopixel(a2);

    console.log("x values: {}, {}", x1, x2);

    for(let x=x1; x<=x2; x++)
    {
        for(let y=0; y<gameheight; y++)
        {
            pixels[(y * gamewidth + x) * 4 + 2] = 255;
        }
    }
}

export function render()
{
    for(let y=0; y<gameheight; y++)
    {
        for(let x=0; x<gamewidth; x++)
        {
            pixels[(y * gamewidth + x) * 4 + 0] = 255 * x / gamewidth;
            pixels[(y * gamewidth + x) * 4 + 1] = 255 * y / gameheight;
            pixels[(y * gamewidth + x) * 4 + 2] = 0;
            pixels[(y * gamewidth + x) * 4 + 3] = 255;
        }
    }
    renderseg(segs[0]);
}