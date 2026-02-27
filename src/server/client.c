#include "client.h"

#include <string.h>
#include <stdio.h>

#include "../net.h"
#include "../netchan.h"

client_t clients[MAX_CLIENT] = {};

void sendtoclients(void)
{

}

void recvfromclients(void)
{
    uint8_t buf[NET_MAX_PACKET_SIZE];
    int dc, size, i;

    while ((size = net_recv(buf, sizeof(buf), &dc)) > 0)
    {
        // find the client this data channel belongs to
        for (i = 0; i < MAX_CLIENT; i++)
            if (clients[i].state != CLSTATE_DC && clients[i].dc_id == dc)
                break;

        if (i >= MAX_CLIENT)
        {
            printf("[sv] packet from unknown dc %d, dropping\n", dc);
            continue;
        }

        netchan_recv(&clients[i].chan, buf, size);
    }
}

bool addclient(int dc_id)
{
    int i;

    for(i=0; i<MAX_CLIENT; i++)
        if(clients[i].state == CLSTATE_DC)
            break;

    if(i >= MAX_CLIENT)
        return false;

    memset(&clients[i], 0, sizeof(client_t));
    clients[i].dc_id = dc_id;
    clients[i].state = CLSTATE_SHAKING;
    clients[i].chan.outseq = 1;
    return true;
}