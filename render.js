import { curmap } from './main.js';
import { clearplanes, renderplanes } from './renderplane.js';
import { drawfullseg, rendersegsclear, wallspans } from './renderseg.js';
import { pixels, gamewidth, gameheight } from './screen.js'
import { nodeside, palettes, patches, textures } from './wad.js';

export const hfov = 90;
export const vfov = hfov * gameheight / gamewidth;

function rendersubsector(num)
{

}

export var viewx = 0;
export var viewy = 0;
export var viewz = 0;

export function setviewpos(x, y, z)
{
    viewx = x;
    viewy = y;
    viewz = z;
}

function drawssector(ssector)
{
    for(let i=ssector.firstseg; i<ssector.firstseg+ssector.nseg; i++)
    {
        drawfullseg(curmap.segs[i]);
    }
}

function rendernode(nodenum)
{
    if(nodenum & 0x8000)
    {
        if(nodenum == -1)
            drawssector(curmap.ssectors[0]);
        else
            drawssector(curmap.ssectors[nodenum & 0x7FFF]);
        
        return;
    }

    const node = curmap.nodes[nodenum];
    const side = nodeside(node, viewx, viewy);

    rendernode(node.children[side]);
    rendernode(node.children[side^1]);
}

export function render()
{
    rendersegsclear();
    clearplanes();

    for(let i=0; i<gamewidth * gameheight * 0; i++)
    {
        pixels[i * 4] = 0;
        pixels[i * 4 + 1] = 0;
        pixels[i * 4 + 2] = 0;
    }

    rendernode(curmap.nodes.length - 1);
    renderplanes();

    // console.log(wallspans);

    // testgraphic();
}