#ifndef _CLIENT_H
#define _CLIENT_H

#include <stdlib.h>

#include "netchan.h"

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
    addr_t addr;
    netchan_t chan;
    clstate_e state;
    int dc_id;
} client_t;

extern client_t clients[MAX_CLIENT];

void sendtoclients(void);
void recvfromclients(void);
bool addclient(int dc_id);

#endif