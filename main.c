#include <stdio.h>
#include <emscripten.h>
#include <SDL.h>
#include <stdint.h>

#include "player.h"

// Global screen dimensions
const int gamewidth = 1280;
const int gameheight = 800;

SDL_Renderer *renderer;
SDL_Texture *screenTexture;
uint32_t *pixels; // Our raw pixel buffer

// Simple example for now: a moving colored rectangle
int rect_x = 10;
int rect_y = 10;
const int rect_size = 50;
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

    for(i=0; i<gamewidth*gameheight; i++)
        pixels[i] = 0xFF000000;

    SDL_UpdateTexture(screenTexture, NULL, pixels, gamewidth * sizeof(uint32_t));

    SDL_RenderCopy(renderer, screenTexture, NULL, NULL);

    SDL_RenderPresent(renderer);

    lastframetime = curtime;
    fpsframes++;
}

int main()
{
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_CreateWindowAndRenderer(gamewidth, gameheight, 0, &window, &renderer);
    screenTexture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      gamewidth, gameheight);
    pixels = malloc(gamewidth * gameheight * sizeof(uint32_t));
    keystates = SDL_GetKeyboardState(NULL);

    lastframetime = lastfpscheck = emscripten_get_now();
    emscripten_set_main_loop(loop, 0, 1);

    free(pixels);
    SDL_DestroyTexture(screenTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}