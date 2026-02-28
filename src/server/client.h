#ifndef _CLIENT_H
#define _CLIENT_H

#include <stdlib.h>

#include "netchan.h"
#include "packets.h"
#include "player.h"

#define MAX_CLIENT 16

typedef int8_t addr_t[4];

typedef enum
{
    CLSTATE_DC=0,
    CLSTATE_SHAKING,
    CLSTATE_CONNECTED,
} clstate_e;

typedef struct
{
    netchan_t chan;
    clstate_e state;
    player_t player;
    char username[USERNAME_LEN];
    int dc;
} client_t;

extern client_t clients[MAX_CLIENT];

void recvfromclients(void);
void sendtoclients(void);
void spawnplayer(client_t* client);

#endif