#include "connection.h"

#include <string.h>
#include <stdio.h>

#include "client.h"
#include "level.h"
#include "net.h"
#include "netchan.h"
#include "packets.h"
#include "render.h"
#include "wad.h"

conn_t serverconn = {};

static void connect(void)
{
    netbuf_t buf;
    char username[USERNAME_LEN];

    memset(&serverconn.chan, 0, sizeof(netchan_t));
    memset(username, 0, sizeof(username));
    strncpy(username, "player", USERNAME_LEN - 1);

    netbuf_init(&buf);
    netbuf_writeu8(&buf, CVS_HANDSHAKE);
    netbuf_writedata(&buf, username, USERNAME_LEN);
    netchan_queue(&serverconn.chan, &buf);
    netbuf_free(&buf);
}

static void* recvclev(void* buf, void* curpos, int len)
{
    int8_t e, m;

    if(serverconn.state != CLSTATE_CONNECTED)
        return curpos + 2;

    e = net_readi8(buf, curpos++, len);
    m = net_readi8(buf, curpos++, len);
    if(netpacketfull)
        return NULL;

    level_load(e, m);

    return curpos;
}

static void* recvshake(void* buf, void* curpos, int len)
{
    int i;

    int nwads;
    char wadname[13];

    serverconn.clientid = net_readi32(buf, curpos, len);
    curpos += 4;
    if(netpacketfull)
        return NULL;

    serverconn.edict = net_readi32(buf, curpos, len);
    curpos += 4;
    if(netpacketfull)
        return NULL;

    nwads = net_readi16(buf, curpos, len);
    curpos += 2;
    if(netpacketfull)
        return NULL;

    for(i=0; i<nwads; i++)
    {
        net_readdata(wadname, 13, buf, curpos, len);
        curpos += 13;
        if(netpacketfull)
            return NULL;

        wadname[12] = 0;
        wad_load(wadname);
    }

    render_init();

    memset(&player, 0, sizeof(player));
    player.mobj = &mobjs[serverconn.edict];

    memset(player.mobj, 0, sizeof(object_t));
    level_placemobj(player.mobj);
    mobjs[serverconn.edict].exists = true;
    

    serverconn.state = CLSTATE_CONNECTED;
    printf("[net] handshake ack, player id %d\n", serverconn.clientid);

    return curpos;
}

static void recvpacket(void *buf, int len)
{
    void *curpos;
    uint8_t packid;

    curpos = netchan_recv(&serverconn.chan, buf, len);
    if(!curpos)
        return;

    while(1)
    {
        packid = net_readu8(buf, curpos++, len);
        if(netpacketfull)
            break;

        switch(packid)
        {
        case SVC_HANDSHAKE:
            if(!(curpos = recvshake(buf, curpos, len)))
                return;
            break;
        case SVC_SERVERFULL:
            printf("[net] server full\n");
            break;
        case SVC_CHANGELEVEL:
            if(!(curpos = recvclev(buf, curpos, len)))
                return;
            break;
        default:
            fprintf(stderr, "[net] bad packet id %d from server\n", packid);
            return;
        }
    }
}

void recvfromserver(void)
{
    uint8_t buf[NET_MAX_PACKET_SIZE];
    int len;

    while((len = net_recv(buf, sizeof(buf), NULL)) > 0)
        recvpacket(buf, len);
}

void sendtoserver(void)
{
    if(!net_connected())
    {
        serverconn.state = CLSTATE_DC;
        return;
    }

    if(serverconn.state == CLSTATE_DC)
    {
        connect();
        serverconn.state = CLSTATE_SHAKING;
    }

    if(serverconn.state != CLSTATE_DC)
        netchan_send(&serverconn.chan, net_server_dc(), NULL);
}
