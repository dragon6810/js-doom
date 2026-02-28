#include "client.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "info.h"
#include "level.h"
#include "net.h"
#include "netchan.h"
#include "wad.h"

objinfo_t dummystate[MAX_MOBJ] = {};

client_t clients[MAX_CLIENT] = {};

static void updategamestate(client_t* cl)
{
    int i;

    int index;
    objinfo_t *gs;

    index = cl->chan.outseq % GAMESTATE_WINDOW;
    gs = cl->gamestates[index];
    for(i=0; i<mobjmax; i++)
        gs[i] = mobjs[i].info;
}

static void addentdeltas(int edict, client_t* cl, netbuf_t* buf)
{
    int fieldflags;
    objinfo_t *gs, *compare, *info;

    info = &mobjs[edict].info;

    if(!cl->chan.inack)
        gs = dummystate;
    else
        gs = cl->gamestates[cl->chan.inack % GAMESTATE_WINDOW];

    compare = &gs[edict];

    fieldflags = 0;
    if(compare->exists != info->exists)
        fieldflags |= FIELD_EXISTS;
    if(compare->x != info->x)
        fieldflags |= FIELD_X;
    if(compare->y != info->y)
        fieldflags |= FIELD_Y;
    if(compare->z != info->z)
        fieldflags |= FIELD_Z;
    if(compare->angle != info->angle)
        fieldflags |= FIELD_ANGLE;
    if(compare->state != info->state)
        fieldflags |= FIELD_STATE;
    if(!compare->exists && info->exists)
        fieldflags |= FIELD_TYPE;

    // ent is the exact same
    if(!fieldflags)
        return;

    netbuf_writeu16(buf, fieldflags);
    if(fieldflags & FIELD_EXISTS)
        netbuf_writeu8(buf, info->exists);
    if(fieldflags & FIELD_X)
        netbuf_writefloat(buf, info->x);
    if(fieldflags & FIELD_Y)
        netbuf_writefloat(buf, info->y);
    if(fieldflags & FIELD_Z)
        netbuf_writefloat(buf, info->z);
    if(fieldflags & FIELD_ANGLE)
        netbuf_writeu32(buf, info->angle);
    if(fieldflags & FIELD_STATE)
        netbuf_writei16(buf, info->state);
    if(fieldflags & FIELD_TYPE)
        netbuf_writei16(buf, info->type);
}

static void buildunreliable(client_t* cl, netbuf_t* buf)
{
    int i;

    netbuf_writeu8(buf, SVC_ENTDELTAS);

    for(i=0; i<mobjmax; i++)
    {
        if(!mobjs[i].info.exists)
            continue;
        addentdeltas(i, cl, buf);
    }

    netbuf_writeu16(buf, 0xFFFF);
}

void sendtoclients(void)
{
    int i;

    netbuf_t unreliable;

    for(i=0; i<MAX_CLIENT; i++)
    {
        if(clients[i].state == CLSTATE_DC)
            continue;
        
        #if 0
        if(clients[i].chan.outseq > clients[i].chan.lastseen)
            continue;
        else
            clients[i].chan.outseq = clients[i].chan.lastseen;
        #endif

        netbuf_init(&unreliable);
        buildunreliable(&clients[i], &unreliable);
        netchan_send(&clients[i].chan, clients[i].dc, &unreliable);
        netbuf_free(&unreliable);

        updategamestate(&clients[i]);
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
    int edict;
    
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
        netbuf_writei32(&reply, 1);
        netbuf_writei32(&reply, 0);
        netbuf_writei16(&reply, 0);
        netbuf_writeu8(&reply, SVC_SERVERFULL);
        net_send(dc, reply.data, reply.len);
        netbuf_free(&reply);
        return;
    }

    clients[i].state = CLSTATE_SHAKING;
    clients[i].dc = dc;
    strcpy(clients[i].username, username);
    
    memset(&clients[i].chan, 0, sizeof(netchan_t));
    netchan_recv(&clients[i].chan, buf, len);

    spawnplayer(&clients[i]);
    edict = clients[i].player.mobj - mobjs;

    netbuf_init(&reply);
    netbuf_writeu8(&reply, SVC_HANDSHAKE);
    netbuf_writei32(&reply, i);
    netbuf_writei32(&reply, edict);

    netbuf_writei16(&reply, nwads);
    for(j=0; j<nwads; j++)
        netbuf_writedata(&reply, wadnames[j], 13);
    netchan_queue(&clients[i].chan, &reply);
    netbuf_free(&reply);

    netbuf_init(&reply);
    netbuf_writeu8(&reply, SVC_CHANGELEVEL);
    netbuf_writeu8(&reply, level_episode);
    netbuf_writeu8(&reply, level_map);
    netchan_queue(&clients[i].chan, &reply);
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

void spawnplayer(client_t* client)
{
    int edict;

    edict = level_findnewedict();
    assert(edict != -1);

    memset(&client->player, 0, sizeof(player_t));
    client->player.mobj = &mobjs[edict];
    level_placemobj(client->player.mobj);

    client->player.mobj->info.type = MT_PLAYER;
    client->player.mobj->info.state = mobjinfo[MT_PLAYER].spawnstate;
}