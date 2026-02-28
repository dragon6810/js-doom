#include "client.h"

#include <string.h>
#include <stdio.h>

#include "net.h"
#include "netchan.h"
#include "wad.h"

client_t clients[MAX_CLIENT] = {};

void sendtoclients(void)
{
    int i;

    for(i=0; i<MAX_CLIENT; i++)
    {
        if(clients[i].state == CLSTATE_DC)
            continue;
        
        if(clients[i].chan.outseq > clients[i].chan.inseq)
            continue;
        else
            clients[i].chan.outseq = clients[i].chan.inseq;

        netchan_send(&clients[i].chan, clients[i].dc, NULL);
    }
}

static void recvpacket(client_t* cl, void* buf, int len)
{
    uint8_t packettype;

    void *curpos;

    curpos = netchan_recv(&cl->chan, buf, len);
    if(!curpos)
        return;

    while(1)
    {
        packettype = net_readu8(buf, curpos++, len);
        if(netpacketfull)
            break;

        switch(packettype)
        {
        case CVS_HANDSHAKE:
            curpos += USERNAME_LEN;
            break;
        default:
            fprintf(stderr, "recvpacket: bad packet id %d from client %d\n", packettype, (int) (cl - clients));
            return;
        }
    }

    if(cl->state == CLSTATE_SHAKING && cl->chan.justgotack)
    {
        cl->state = CLSTATE_CONNECTED; // handshake acknowledged
        printf("handshake complete with client %d\n", (int) (cl - clients));
    }
}

static void processhandshake(int dc, void* buf, int len)
{
    int i, j;

    void *curpos;
    int32_t seq;
    int8_t packid;
    char *username;
    netbuf_t reply;
    
    curpos = buf;
    seq = net_readi32(buf, curpos, len);
    curpos += 4;
    if(netpacketfull || !(seq & 0x80000000))
        return;
    
    curpos += 6; // skip ack and qport

    packid = net_readu8(buf, curpos++, len);
    if(netpacketfull || packid != CVS_HANDSHAKE)
        return;
    
    username = curpos;
    curpos += USERNAME_LEN;

    for(i=0; i<USERNAME_LEN; i++)
        if(!username[i])
            break;
    if(i >= USERNAME_LEN)
        return;

    for(i=0; i<MAX_CLIENT; i++)
        if(clients[i].state == CLSTATE_DC)
            break;
    if(i >= MAX_CLIENT)
    {
        netbuf_init(&reply);
        netbuf_writei32(&reply, 0);
        netbuf_writei32(&reply, 0);
        netbuf_writei16(&reply, 0);
        netbuf_writeu8(&reply, SVC_SERVERFULL);
        net_send(dc, reply.data, reply.len);
        netbuf_free(&reply);
        return;
    }

    printf("responding to client %d handshake\n", i);

    clients[i].state = CLSTATE_SHAKING;
    clients[i].dc = dc;
    strcpy(clients[i].username, username);
    
    memset(&clients[i].chan, 0, sizeof(netchan_t));
    netchan_recv(&clients[i].chan, buf, len);

    netbuf_init(&reply);
    netbuf_writeu8(&reply, SVC_HANDSHAKE);
    netbuf_writei32(&reply, i);

    netbuf_writei16(&reply, nwads);
    for(j=0; j<nwads; j++)
        netbuf_writedata(&reply, wadnames[j], 13);

    clients[i].chan.reliablesize = reply.len;
    memcpy(clients[i].chan.reliable, reply.data, reply.len);

    netbuf_free(&reply);
}

void recvfromclients(void)
{
    int i;

    uint8_t buf[NET_MAX_PACKET_SIZE];
    int dc, len;

    while((len = net_recv(buf, sizeof(buf), &dc)) > 0)
    {
        for (i=0; i<MAX_CLIENT; i++)
            if (clients[i].state != CLSTATE_DC && clients[i].dc == dc)
                break;

        if (i >= MAX_CLIENT)
        {
            processhandshake(dc, buf, len);
            continue;
        }

        recvpacket(&clients[i], buf, len);
    }
}