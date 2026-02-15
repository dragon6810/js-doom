const gamecanvas = document.getElementById('gamecanvas');
const gamectx = gamecanvas.getContext('2d');

gamectx.globalCompositeOperation = 'copy'; 

export const gamewidth = gamecanvas.width;
export const gameheight = gamecanvas.height;
const imagedata = gamectx.createImageData(gamewidth, gameheight);
export const pixels = imagedata.data;
// A, B, G, R
export const pixels32 = new Uint32Array(imagedata.data.buffer);

export function display()
{
    gamectx.putImageData(imagedata, 0, 0);
}

export function clear()
{
    // 4. Optimize clear
    // Instead of a loop, we use the native .fill() method.
    // 0xFF000000 represents Opaque Black in Little Endian (A=FF, B=00, G=00, R=00)
    pixels32.fill(0xFF000000); 
}