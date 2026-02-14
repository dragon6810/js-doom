import { curmap } from './main.js';
import { drawfullseg } from './renderseg.js';
import { pixels, gamewidth, gameheight } from './screen.js'
import { palettes, patches, textures } from './wad.js';

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

function nodeside(node, x, y)
{
    const dx = x - node.x;
    const dy = y - node.y;

    if(node.dx * node.dy * dx * dy < 0)
    {
        if(node.dy * dx < 0)
            return 1;
        return 0;
    }

    if(node.dy * dx >= node.dx * dy)
        return 0;
    return 1;
}

function drawssector(ssector)
{
    for(let i=ssector.firstseg; i<ssector.firstseg+ssector.nseg; i++)
        drawfullseg(curmap.segs[i]);
}

function rendernode(nodenum)
{
    if(nodenum < 0)
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
    rendernode(node.children[side ^ 1]);
}

export function render()
{
    for(let i=0; i<gamewidth * gameheight; i++)
    {
        pixels[i * 4] = 0;
        pixels[i * 4 + 1] = 0;
        pixels[i * 4 + 2] = 0;
    }
    rendernode(curmap.nodes.length - 1);

    // testgraphic();
}