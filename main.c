#include <stdio.h>
#include <emscripten.h>
#include <SDL.h>
#include <stdint.h> 

#include "level.h"
#include "player.h"
#include "render.h"
#include "screen.h"
#include "wad.h"

int screenwidth;
int screenheight;

SDL_Renderer *renderer;
SDL_Texture *screenTexture;
uint32_t *pixels;

const Uint8* keystates;

double lastframetime, lastfpscheck;
int fpsframes;

playercmd_t inputcmd;

void gatherinput(void)
{
    const angle_t turnspeed = DEGTOANG(180.0 * inputcmd.frametime);

    inputcmd.flags = 0;

    SDL_PumpEvents();
    if(keystates[SDL_SCANCODE_W]) inputcmd.flags |= CMD_FORWARD;
    if(keystates[SDL_SCANCODE_S]) inputcmd.flags |= CMD_BACK;
    if(keystates[SDL_SCANCODE_A]) inputcmd.flags |= CMD_LEFT;
    if(keystates[SDL_SCANCODE_D]) inputcmd.flags |= CMD_RIGHT;

    inputcmd.deltaangle = 0;
    if(keystates[SDL_SCANCODE_LEFT]) inputcmd.deltaangle += turnspeed;
    if(keystates[SDL_SCANCODE_RIGHT]) inputcmd.deltaangle -= turnspeed;
}

void loop(void)
{
    int i;

    double curtime, frametime;

    curtime = emscripten_get_now();
    frametime = inputcmd.frametime = (curtime - lastframetime) / 1000.0;

    if(curtime - lastfpscheck > 1000)
    {
        printf("%d fps\n", fpsframes);
        fpsframes = 0;
        lastfpscheck = curtime;
    }

    gatherinput();
    player_docmd(&player, &inputcmd);

    for(i=0; i<screenwidth*screenheight; i++)
        pixels[i] = 0xFF000000;

    render();

    SDL_UpdateTexture(screenTexture, NULL, pixels, screenwidth * sizeof(uint32_t));

    SDL_RenderCopy(renderer, screenTexture, NULL, NULL);

    SDL_RenderPresent(renderer);

    lastframetime = curtime;
    fpsframes++;
}

EMSCRIPTEN_KEEPALIVE
void screen_init(int width, int height)
{
    screenwidth = width;
    screenheight = height;

    SDL_Window *window;
    SDL_CreateWindowAndRenderer(screenwidth, screenheight, 0, &window, &renderer);

    screenTexture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      screenwidth, screenheight);

    pixels = (uint32_t*) malloc(screenwidth * screenheight * sizeof(uint32_t));
}

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    
    wad_load("doom.wad");
    wad_setpalette(0);

    level_load(1, 1);
    
    keystates = SDL_GetKeyboardState(NULL);

    emscripten_set_main_loop(loop, 0, 1);

    // The following code is now effectively dead, but we keep it for potential future cleanup.
    free(pixels);
    SDL_DestroyTexture(screenTexture);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();

    return 0;
}