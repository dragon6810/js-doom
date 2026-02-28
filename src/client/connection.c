#include "connection.h"

#include <string.h>
#include <stdio.h>

#include "net.h"
#include "netchan.h"
#include "packets.h"

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

    serverconn.chan.reliablesize = buf.len;
    memcpy(serverconn.chan.reliable, buf.data, buf.len);
    netbuf_free(&buf);
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
            serverconn.playerid = net_readi32(buf, curpos, len);
            curpos += 4;
            if(netpacketfull)
                return;
            serverconn.state = CLSTATE_CONNECTED;
            printf("[net] handshake ack, player id %d\n", serverconn.playerid);
            return;

        case SVC_SERVERFULL:
            printf("[net] server full\n");
            return;

        default:
            fprintf(stderr, "[net] bad packet id %d from server\n", packid);
            return;
        }
    }
}

void recvfromserver(void)
{
    uint8_t buf[NET_MAX_PACKET_SIZE];
    int     len;

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
