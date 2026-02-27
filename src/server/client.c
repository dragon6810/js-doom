#include "client.h"

#include <string.h>

client_t clients[MAX_CLIENT] = {};

void sendtoclients(void)
{

}

void recvfromclients(void)
{
    
}

bool addclient(addr_t addr)
{
    int i;

    for(i=0; i<MAX_CLIENT; i++)
        if(clients[i].state == CLSTATE_DC)
            break;

    if(i >= MAX_CLIENT)
        return false;

    memset(&clients[i], 0, sizeof(client_t));
    clients[i].state = CLSTATE_SHAKING;
    clients[i].chan.outseq = 1;
    return true;
}