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