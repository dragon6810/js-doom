#ifndef _CLIENT_H
#define _CLIENT_H

#include <stdlib.h>

#include "level.h"
#include "netchan.h"
#include "packets.h"
#include "player.h"

#define MAX_CLIENT 16
#define GAMESTATE_WINDOW 32
#define CLIENT_TIMEOUT 30

typedef int8_t addr_t[4];

typedef enum
{
    CLSTATE_DC=0,
    CLSTATE_SHAKING,
    CLSTATE_CONNECTED,
} clstate_e;

typedef struct
{
    gamestate_t gamestates[GAMESTATE_WINDOW];

    uint8_t buttons;

    netchan_t chan;
    clstate_e state;
    player_t player;
    char username[USERNAME_LEN];
    int dc;
    uint32_t lastrecv;
} client_t;

extern gamestate_t dummystate;

extern client_t clients[MAX_CLIENT];

void allocgamestatesectors(void);
void recvfromclients(void);
void sendtoclients(void);
void spawnplayer(client_t* client);
void disconnectclient(int i);

#endif