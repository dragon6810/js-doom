#ifndef _CLIENT_H
#define _CLIENT_H

#include "player.h"

typedef enum
{
    CLGS_MAINMENU=0,
    CLGS_CONNECTING,
    CLGS_CONNECTED,
    CLGS_MELTING,

    NUM_CLGS,
    
    CLGS_NONE // for pendgs
} clgamestate_e;

extern player_t player;

extern bool sendinputs;
extern playercmd_t inputcmd;

extern clgamestate_e curgs, pendgs;

#endif