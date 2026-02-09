const gamecanvas = document.getElementById('gamecanvas');
const gamectx = gamecanvas.getContext('2d');

export const gamewidth = gamecanvas.width;
export const gameheight = gamecanvas.height;
const imagedata = gamectx.createImageData(gamewidth, gameheight);
export const pixels = imagedata.data;

export function display()
{
    gamectx.putImageData(imagedata, 0, 0);
}

export function clear()
{
    for(let y=0; y<gameheight; y++)
    {
        for(let x=0; x<gamewidth; x++)
        {
            pixels[(y * gamewidth + x) * 4 + 0] = 0;
            pixels[(y * gamewidth + x) * 4 + 1] = 0;
            pixels[(y * gamewidth + x) * 4 + 2] = 0;
            pixels[(y * gamewidth + x) * 4 + 3] = 255;
        }
    }
}