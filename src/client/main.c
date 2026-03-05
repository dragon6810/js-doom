#include <arpa/inet.h>
#include <emscripten.h>
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "connection.h"
#include "level.h"
#include "net.h"
#include "player.h"
#include "predict.h"
#include "render.h"
#include "screen.h"
#include "stbar.h"
#include "think.h"
#include "wad.h"

int screenwidth;
int screenheight;

SDL_Renderer *renderer;
SDL_Texture *screenTexture;
uint32_t *pixels;

const Uint8* keystates;

float starttime;
double lastframetime, lastfpscheck;
int fpsframes;

bool sendinputs;
playercmd_t inputcmd;

player_t player = {};

void douse(void)
{
    netbuf_t netbuf;

    netbuf_init(&netbuf);
    netbuf_writeu8(&netbuf, CSV_USE);
    netchan_queue(&serverconn.chan, &netbuf);
    netbuf_free(&netbuf);
}

bool uselastframe = false;

void gatherinput(void)
{
    const angle_t turnspeed = DEGTOANG(180.0 * inputcmd.frametime);

    sendinputs = true;
    inputcmd.flags = 0;

    SDL_PumpEvents();
    if(keystates[SDL_SCANCODE_W]) inputcmd.flags |= CMD_FORWARD;
    if(keystates[SDL_SCANCODE_S]) inputcmd.flags |= CMD_BACK;
    if(keystates[SDL_SCANCODE_A]) inputcmd.flags |= CMD_LEFT;
    if(keystates[SDL_SCANCODE_D]) inputcmd.flags |= CMD_RIGHT;

    inputcmd.angle = mobjs[serverconn.edict].info.angle;
    if(keystates[SDL_SCANCODE_LEFT]) inputcmd.angle += turnspeed;
    if(keystates[SDL_SCANCODE_RIGHT]) inputcmd.angle -= turnspeed;

    if(keystates[SDL_SCANCODE_SPACE])
    {
        if(!uselastframe)
        {
            douse();
            uselastframe = true;
        }
    }
    else
        uselastframe = false;
}

float lastplayerz = INFINITY, playerz;

void loop(void)
{
    int i;

    int num;

    double curtime, frametime, progtime;

    curtime = emscripten_get_now();
    progtime = curtime / 1000.0 - starttime;
    frametime = (curtime - lastframetime) / 1000.0;
    if(frametime > 0.1) frametime = 0.1;
    inputcmd.frametime = frametime;

    if(curtime - lastfpscheck > 1000)
    {
        printf("%d fps\n", fpsframes);
        fpsframes = 0;
        lastfpscheck = curtime;
    }

    recvfromserver(curtime / 1000.0);

    think(frametime);

    if(serverconn.state == CLSTATE_CONNECTED)
    {
        sendinputs = false;
        if(level_episode != -1 && level_map != -1)
        {
            predictplayer();
            interpsectors(curtime / 1000.0);
            interpentities(curtime / 1000.0);

            gatherinput();

            playerz = mobjs[serverconn.edict].info.z;
            if(lastplayerz != INFINITY && playerz > lastplayerz)
            {
                pviewheight -= playerz - lastplayerz;
                deltaviewheight = (41.0 - pviewheight) / 8;
            }
            lastplayerz = playerz;

            viewx = mobjs[serverconn.edict].info.x;
            viewy = mobjs[serverconn.edict].info.y;
            viewz = player_getviewheight(&mobjs[serverconn.edict], progtime, frametime);
            viewangle = mobjs[serverconn.edict].info.angle;
            render();
            stbar_draw();
        }
    }

    sendtoserver();

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
    starttime = emscripten_get_now() / 1000.0;

    SDL_Init(SDL_INIT_VIDEO);

    net_init();
    render_init();
    stbar_init();
    player_init();

    keystates = SDL_GetKeyboardState(NULL);

    emscripten_set_main_loop(loop, 0, 1);

    // The following code is now effectively dead, but we keep it for potential future cleanup.
    free(pixels);
    SDL_DestroyTexture(screenTexture);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();

    return 0;
}