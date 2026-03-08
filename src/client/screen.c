#include "screen.h"

#include <emscripten.h>
#include <SDL.h>

SDL_Renderer *renderer;
SDL_Texture *screenTexture;
uint32_t *pixels;

screen_t screens[NUM_SCR] = {};

color_t *curpalette = NULL;

EMSCRIPTEN_KEEPALIVE
void screen_init(int width, int height)
{
    SDL_Window *window;

    screenwidth = width;
    screenheight = height;

    SDL_CreateWindowAndRenderer(screenwidth, screenheight, 0, &window, &renderer);

    screenTexture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      screenwidth, screenheight);

    pixels = (uint32_t*) malloc(screenwidth * screenheight * sizeof(uint32_t));
}

void screen_initscreens(void)
{
    float scale;

    scale = screenheight / 200.0;

    if(screens[SCR_LVL].pixels)
        free(screens[SCR_LVL].pixels);
    screens[SCR_LVL].w = screenwidth;
    screens[SCR_LVL].h = screenheight - 32.0 * scale;
    screens[SCR_LVL].x = screens[SCR_LVL].y = 0;
    screens[SCR_LVL].pixels = malloc(screens[SCR_LVL].w * screens[SCR_LVL].h * sizeof(uint8_t));

    if(screens[SCR_ST].pixels)
        free(screens[SCR_ST].pixels);
    screens[SCR_ST].w = screenwidth;
    screens[SCR_ST].h = 32.0 * scale;
    screens[SCR_ST].x = 0;
    screens[SCR_ST].y = screenheight - 32.0 * scale;
    screens[SCR_ST].pixels = malloc(screens[SCR_ST].w * screens[SCR_ST].h * sizeof(uint8_t));
}

void screen_present(void)
{
    int i, x, y, src, dst;
    color_t *col;

    if(!curpalette)
        return;

    for(i=0; i<NUM_SCR; i++)
    {
        for(y=src=0; y<screens[i].h; y++)
        {
            for(x=0; x<screens[i].w; x++, src++)
            {
                dst = (y + screens[i].y) * screenwidth + x + screens[i].x;
                col = &curpalette[screens[i].pixels[src]];
                pixels[dst] = (int) col->r << 16 | (int) col->g << 8 | (int) col->b;
            }
        }
    }

    SDL_UpdateTexture(screenTexture, NULL, pixels, screenwidth * sizeof(uint32_t));
    SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
    SDL_RenderPresent(renderer);
}
