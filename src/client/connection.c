#include "connection.h"

#include <emscripten.h>
#include <string.h>
#include <stdio.h>

#define HANDSHAKE_TIMEOUT_MS 5000.0
#define RETRY_DELAY_MS       3000.0

#include "client.h"
#include "level.h"
#include "net.h"
#include "netchan.h"
#include "packets.h"
#include "predict.h"
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

static void* recventdeltas(void* buf, void* curpos, int len)
{
    int edict;
    int fields;
    objinfo_t *info;

    while(1)
    {
        edict = net_readu16(buf, curpos, len);
        curpos += 2;
        if(edict == 0xFFFF || netpacketfull)
            break;

        fields = net_readu16(buf, curpos, len);
        curpos += 2;
        if(netpacketfull)
            break;

        info = &mobjs[edict].info;
        if(fields & FIELD_EXISTS)
        {
            info->exists = net_readu8(buf, curpos, len);
            curpos += 1;
        }
        if(fields & FIELD_X)
        {
            info->x = net_readfloat(buf, curpos, len);
            curpos += 4;
        }
        if(fields & FIELD_Y)
        {
            info->y = net_readfloat(buf, curpos, len);
            curpos += 4;
        }
        if(fields & FIELD_Z)
        {
            info->z = net_readfloat(buf, curpos, len);
            curpos += 4;
        }
        if(fields & FIELD_ANGLE)
        {
            info->angle = net_readu32(buf, curpos, len);
            curpos += 4;
        }
        if(fields & FIELD_STATE)
        {
            info->state = net_readi16(buf, curpos, len);
            curpos += 2;
        }
        if(fields & FIELD_TYPE)
        {
            info->type = net_readi16(buf, curpos, len);
            curpos += 2;
        }
        if(fields & FIELD_XVEL)
        {
            info->xvel = net_readfloat(buf, curpos, len);
            curpos += 4;
        }
        if(fields & FIELD_YVEL)
        {
            info->yvel = net_readfloat(buf, curpos, len);
            curpos += 4;
        }
        if(fields & FIELD_ZVEL)
        {
            info->zvel = net_readfloat(buf, curpos, len);
            curpos += 4;
        }

        level_unplacemobj(&mobjs[edict]);
        if(!info->exists)
            memset(info, 0, sizeof(*info));
        else
            level_placemobj(&mobjs[edict]);
    }

    if(netpacketfull)
        return NULL;

    return curpos;
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
    mobjs[serverconn.edict].info.exists = true;

    serverconn.state = CLSTATE_CONNECTED;
    printf("[net] handshake ack, client id %d, edict id %d\n", serverconn.clientid, serverconn.edict);

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
            serverconn.state = CLSTATE_DC;
            serverconn.nextattempt = emscripten_get_now() + RETRY_DELAY_MS;
            break;
        case SVC_CHANGELEVEL:
            if(!(curpos = recvclev(buf, curpos, len)))
                return;
            break;
        case SVC_ENTDELTAS:
            if(!(curpos = recventdeltas(buf, curpos, len)))
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

void buildunreliable(netbuf_t* buf)
{
    if(!sendinputs)
        return;
    netbuf_writeu8(buf, CSV_INPUT);
    netbuf_writeu8(buf, inputcmd.flags);
    netbuf_writeu32(buf, inputcmd.angle);
    netbuf_writefloat(buf, inputcmd.frametime);
}

void sendtoserver(void)
{
    double now;
    netbuf_t buf;

    if(!net_connected())
    {
        serverconn.state = CLSTATE_DC;
        return;
    }

    now = emscripten_get_now();

    if(serverconn.state == CLSTATE_DC)
    {
        if(now < serverconn.nextattempt)
            return;
        connect();
        serverconn.shaketimer = now;
        serverconn.state = CLSTATE_SHAKING;
    }

    if(serverconn.state == CLSTATE_SHAKING &&
       now - serverconn.shaketimer > HANDSHAKE_TIMEOUT_MS)
    {
        printf("[net] handshake timed out, retrying...\n");
        serverconn.state = CLSTATE_DC;
        serverconn.nextattempt = now + RETRY_DELAY_MS;
        return;
    }

    if(serverconn.state != CLSTATE_DC)
    {
        netbuf_init(&buf);
        buildunreliable(&buf);
        netchan_send(&serverconn.chan, net_server_dc(), &buf);
        netbuf_free(&buf);

        if(sendinputs)
            inputwindow[serverconn.chan.outseq % PRED_WINDOW] = inputcmd;
        else
            memset(&inputwindow[serverconn.chan.outseq % PRED_WINDOW], 0, sizeof(playercmd_t));
    }
}
