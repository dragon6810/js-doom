#include <arpa/inet.h>
#include <emscripten.h>
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "connection.h"
#include "draw.h"
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

    think(frametime, progtime);

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
            render(progtime);
            stbar_draw();
        }
    }

    sendtoserver();

    screen_present();

    lastframetime = curtime;
    fpsframes++;
}

int main()
{
    starttime = emscripten_get_now() / 1000.0;

    SDL_Init(SDL_INIT_VIDEO);

    net_init();
    render_init();
    stbar_init();
    player_init();
    draw_init();

    stbar_makethink();

    screen_initscreens();

    keystates = SDL_GetKeyboardState(NULL);

    emscripten_set_main_loop(loop, 0, 1);

    return 0;
}