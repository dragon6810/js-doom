import { pixels, gamewidth, gameheight } from './screen.js'

export function render()
{
    for(let i=0; i<gamewidth*gameheight; i++)
    {
        pixels[i * 4] = Math.random() * 255;
        pixels[i * 4 + 1] = Math.random() * 255;
        pixels[i * 4 + 2] = Math.random() * 255;
        pixels[i * 4 + 3] = 255;
    }
}