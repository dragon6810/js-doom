#include "client.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

int net_recv_disconnect(int *dc_out);

#include "info.h"
#include "level.h"
#include "net.h"
#include "netchan.h"
#include "player.h"
#include "wad.h"

gamestate_t dummystate = {};

client_t clients[MAX_CLIENT] = {};

static void updategamestate(client_t* cl)
{
    int i;

    int index;
    gamestate_t *gs;

    index = cl->chan.outseq % GAMESTATE_WINDOW;
    gs = &cl->gamestates[index];
    gs->maxmobj = mobjmax;
    for(i=0; i<=mobjmax; i++)
        gs->mobjs[i] = mobjs[i].info;
    for(i=0; i<nsectors; i++)
    {
        gs->sectorinfos[i].floorheight = sectors[i].floorheight;
        gs->sectorinfos[i].ceilheight = sectors[i].ceilheight;
    }
}

void allocgamestatesectors(void)
{
    int i, j;

    free(dummystate.sectorinfos);
    dummystate.sectorinfos = calloc(nsectors, sizeof(sectorinfo_t));
    for(i=0; i<MAX_CLIENT; i++)
        for(j=0; j<GAMESTATE_WINDOW; j++)
        {
            free(clients[i].gamestates[j].sectorinfos);
            clients[i].gamestates[j].sectorinfos = calloc(nsectors, sizeof(sectorinfo_t));
        }
}

static void addentdeltas(int edict, client_t* cl, netbuf_t* buf)
{
    bool spawned;
    int fieldflags;
    gamestate_t *gs;
    objinfo_t *compare, *info;

    info = &mobjs[edict].info;

    if(!cl->chan.inack)
        gs = &dummystate;
    else
        gs = &cl->gamestates[cl->chan.inack % GAMESTATE_WINDOW];

    compare = &gs->mobjs[edict];

    if(!info->exists && !compare->exists)
        return;

    spawned = info->exists && !compare->exists;

    fieldflags = 0;
    if(compare->exists != info->exists)
        fieldflags |= FIELD_EXISTS;
    if(info->exists && (spawned || compare->x != info->x))
        fieldflags |= FIELD_X;
    if(info->exists && (spawned || compare->y != info->y))
        fieldflags |= FIELD_Y;
    if(info->exists && (spawned || compare->z != info->z))
        fieldflags |= FIELD_Z;
    if(info->exists && (spawned || compare->angle != info->angle))
        fieldflags |= FIELD_ANGLE;
    if(info->exists && (spawned || compare->state != info->state))
        fieldflags |= FIELD_STATE;
    if(info->exists && (spawned || compare->type != info->type))
        fieldflags |= FIELD_TYPE;
    if(info->exists && (spawned || compare->xvel != info->xvel))
        fieldflags |= FIELD_XVEL;
    if(info->exists && (spawned || compare->yvel != info->yvel))
        fieldflags |= FIELD_YVEL;
    if(info->exists && (spawned || compare->zvel != info->zvel))
        fieldflags |= FIELD_ZVEL;

    // ent is the exact same
    if(!fieldflags)
        return;

    netbuf_writeu16(buf, edict);
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
    if(fieldflags & FIELD_XVEL)
        netbuf_writefloat(buf, info->xvel);
    if(fieldflags & FIELD_YVEL)
        netbuf_writefloat(buf, info->yvel);
    if(fieldflags & FIELD_ZVEL)
        netbuf_writefloat(buf, info->zvel);
}

static void addsectordeltas(int sectornum, client_t* cl, netbuf_t* buf)
{
    int fieldflags;
    bool firstsend;
    gamestate_t *gs;
    sectorinfo_t *compare;
    sectorinfo_t info;

    info.floorheight = sectors[sectornum].floorheight;
    info.ceilheight = sectors[sectornum].ceilheight;

    if(!cl->chan.inack)
        gs = &dummystate;
    else
        gs = &cl->gamestates[cl->chan.inack % GAMESTATE_WINDOW];

    compare = &gs->sectorinfos[sectornum];

    fieldflags = 0;
    if(compare->floorheight != info.floorheight)
        fieldflags |= SFIELD_FLOOR;
    if(compare->ceilheight != info.ceilheight)
        fieldflags |= SFIELD_CEIL;

    if(!fieldflags)
        return;

    netbuf_writeu16(buf, sectornum);
    netbuf_writeu8(buf, fieldflags);
    if(fieldflags & SFIELD_FLOOR)
        netbuf_writefloat(buf, info.floorheight);
    if(fieldflags & SFIELD_CEIL)
        netbuf_writefloat(buf, info.ceilheight);
}

static void buildunreliable(client_t* cl, netbuf_t* buf)
{
    int i;

    netbuf_writeu8(buf, SVC_ENTDELTAS);

    for(i=0; i<=mobjmax; i++)
        addentdeltas(i, cl, buf);

    netbuf_writeu16(buf, 0xFFFF);

    for(i=0; i<nsectors; i++)
        addsectordeltas(i, cl, buf);

    netbuf_writeu16(buf, 0xFFFF);
}

void disconnectclient(int i)
{
    client_t *cl = &clients[i];

    if(cl->state == CLSTATE_DC)
        return;

    if(cl->player.mobj)
    {
        level_unplacemobj(cl->player.mobj);
        memset(&cl->player.mobj->info, 0, sizeof(objinfo_t));
    }

    cl->state = CLSTATE_DC;
    printf("client %d (%s) disconnected\n", i, cl->username);
}

void sendtoclients(void)
{
    int i;

    netbuf_t unreliable;

    for(i=0; i<MAX_CLIENT; i++)
    {
        if(clients[i].state == CLSTATE_DC)
            continue;
        
        #if 1
        if(clients[i].chan.outseq > clients[i].chan.lastseen)
            continue;
        else
            clients[i].chan.outseq = clients[i].chan.lastseen;
        #endif

        netbuf_init(&unreliable);
        buildunreliable(&clients[i], &unreliable);
        if(netchan_send(&clients[i].chan, clients[i].dc, &unreliable))
            updategamestate(&clients[i]);
        netbuf_free(&unreliable);
    }
}

void* recvuse(client_t* cl, void* buf, void* curpos, int len)
{
    player_use(&cl->player);
    return curpos;
}

void* recvinput(client_t* cl, void* buf, void* curpos, int len)
{
    playercmd_t cmd;

    cmd.flags = net_readu8(buf, curpos++, len);
    cmd.angle = net_readu32(buf, curpos, len);
    curpos += 4;
    cmd.frametime = net_readfloat(buf, curpos, len);
    curpos += 4;
    
    if(netpacketfull)
        return NULL;

    cl->buttons = cmd.flags;
    player_docmd(cl->player.mobj, &cmd);

    return curpos;
}

static void recvpacket(client_t* cl, void* buf, int len)
{
    uint8_t packettype;

    void *curpos;

    curpos = netchan_recv(&cl->chan, buf, len);
    if(!curpos)
        return;

    cl->lastrecv = (uint32_t)time(NULL);

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
        case CSV_INPUT:
            if(!(curpos = recvinput(cl, buf, curpos, len)))
                return;
            break;
        case CSV_USE:
            if(!(curpos = recvuse(cl, buf, curpos, len)))
                return;
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
    clients[i].lastrecv = (uint32_t)time(NULL);
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
    int i, dc, len;
    uint32_t now;
    uint8_t buf[NET_MAX_PACKET_SIZE];

    while(net_recv_disconnect(&dc))
    {
        for(i=0; i<MAX_CLIENT; i++)
            if(clients[i].state != CLSTATE_DC && clients[i].dc == dc)
                disconnectclient(i);
    }

    now = (uint32_t)time(NULL);
    for(i=0; i<MAX_CLIENT; i++)
    {
        if(clients[i].state == CLSTATE_DC)
            continue;

        clients[i].buttons = 0;
        if(now - clients[i].lastrecv > CLIENT_TIMEOUT)
        {
            printf("client %d timed out\n", i);
            disconnectclient(i);
        }
    }

    while((len = net_recv(buf, sizeof(buf), &dc)) > 0)
    {
        for(i=0; i<MAX_CLIENT; i++)
            if(clients[i].state != CLSTATE_DC && clients[i].dc == dc)
                break;

        if(i >= MAX_CLIENT)
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
    memset(client->player.mobj, 0, sizeof(object_t));
    client->player.mobj->info.exists = true;
    client->player.mobj->info.type = MT_PLAYER;
    client->player.mobj->info.x = dmstarts[0].x;
    client->player.mobj->info.y = dmstarts[0].y;
    client->player.mobj->info.angle = dmstarts[0].angle;
    level_placemobj(client->player.mobj);
    client->player.mobj->info.z = client->player.mobj->ssector->sector->floorheight;
    client->player.mobj->info.state = mobjinfo[MT_PLAYER].spawnstate;
}