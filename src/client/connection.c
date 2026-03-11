#include "connection.h"

#include <emscripten.h>
#include <stdlib.h>
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
#include "snd.h"
#include "stbar.h"
#include "wad.h"

conn_t serverconn = {};
float lastrecvtime = -1.0;

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

static void* recvsetplayedict(void* buf, void* curpos, int len)
{
    int edict;

    edict = net_readi32(buf, curpos, len);
    curpos += 4;
    if(netpacketfull)
        return NULL;

    if(!INRANGE(edict, 0, MAX_MOBJ-1))
        return curpos;

    serverconn.edict = edict;
    player.mobj = &mobjs[edict];
    player_initinfo(&player.info);

    memset(player.mobj, 0, sizeof(object_t));
    mobjs[serverconn.edict].info.type = MT_PLAYER;
    mobjs[serverconn.edict].info.exists = true;

    if(player.thinker)
        freethinker(player.thinker);
    player_addthinker(&player);

    weapon_initstate(&startwpn);
    player.info.weapon = startwpn;

    return curpos;
}

static void* recvsectordeltas(void* buf, void* curpos, int len)
{
    int sectornum;
    int fields;
    sectorinfo_t *info;

    while(1)
    {
        sectornum = net_readu16(buf, curpos, len);
        curpos += 2;
        if(sectornum == 0xFFFF || netpacketfull)
            break;

        fields = net_readu8(buf, curpos, len);
        curpos += 1;
        if(netpacketfull)
            break;

        info = newgs.sectorinfos ? &newgs.sectorinfos[sectornum] : NULL;
        if(fields & SFIELD_FLOOR)
        {
            float v = net_readfloat(buf, curpos, len);
            curpos += 4;
            if(info) info->floorheight = v;
        }
        if(fields & SFIELD_CEIL)
        {
            float v = net_readfloat(buf, curpos, len);
            curpos += 4;
            if(info) info->ceilheight = v;
        }
    }

    if(netpacketfull)
        return NULL;

    return curpos;
}

static void* recvplayerdeltas(void* buf, void* curpos, int len)
{
    int fields;
    playerinfo_t info;

    fields = net_readu16(buf, curpos, len);
    curpos += 2;
    if(netpacketfull)
        return NULL;

    info = player.info;
    if(fields & PFIELD_FLAGS)
    {
        info.flags = net_readu8(buf, curpos, len);
        curpos += 1;
    }
    if(fields & PFIELD_ARMOR)
    {
        info.armor = net_readu16(buf, curpos, len);
        curpos += 2;
    }
    if(fields & PFIELD_WEAPONS)
    {
        info.weapons = net_readu8(buf, curpos, len);
        curpos += 1;
    }
    if(fields & PFIELD_BULLETS)
    {
        info.ammo[AMMO_BUL] = net_readu16(buf, curpos, len);
        curpos += 2;
    }
    if(fields & PFIELD_SHELLS)
    {
        info.ammo[AMMO_SHEL] = net_readu16(buf, curpos, len);
        curpos += 2;
    }
    if(fields & PFIELD_ROCKETS)
    {
        info.ammo[AMMO_ROCK] = net_readu16(buf, curpos, len);
        curpos += 2;
    }
    if(fields & PFIELD_CELLS)
    {
        info.ammo[AMMO_CELL] = net_readu16(buf, curpos, len);
        curpos += 2;
    }
    if(fields & PFIELD_FRAGS)
    {
        info.frags = net_readu16(buf, curpos, len);
        curpos += 2;
    }
    if(fields & PFIELD_CURWPN)
    {
        startwpn.cur = net_readu8(buf, curpos, len);
        curpos += 1;
    }
    if(fields & PFIELD_PENDWPN)
    {
        startwpn.pend = net_readu8(buf, curpos, len);
        curpos += 1;
    }
    if(fields & PFIELD_WPNST)
    {
        startwpn.state = net_readi16(buf, curpos, len);
        curpos += 2;
    }
    if(fields & PFIELD_WPNTIME)
    {
        startwpn.time = net_readfloat(buf, curpos, len);
        curpos += 4;
    }

    if(netpacketfull)
        return NULL;

    player.info = info;

    return curpos;
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

        if(edict > newgs.maxmobj)
            newgs.maxmobj = edict;

        info = &newgs.mobjs[edict];
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
        if(fields & FIELD_COLOR)
        {
            info->color = net_readu8(buf, curpos, len);
            curpos += 1;
        }
        if(fields & FIELD_HEALTH)
        {
            info->health = net_readi16(buf, curpos, len);
            curpos += 2;
        }
        if(fields & FIELD_FLAGS)
        {
            info->flags = net_readi32(buf, curpos, len);
            curpos += 4;
        }
        if(fields & FIELD_HEIGHT)
        {
            info->height = net_readi16(buf, curpos, len);
            curpos += 2;
        }

        if(!info->exists)
            memset(info, 0, sizeof(*info));
    }

    if(netpacketfull)
        return NULL;

    return recvsectordeltas(buf, curpos, len);
}

static void* recvclev(void* buf, void* curpos, int len)
{
    int i;
    int8_t e, m;

    if(serverconn.state != CLSTATE_CONNECTED)
        return curpos + 2;

    e = net_readi8(buf, curpos++, len);
    m = net_readi8(buf, curpos++, len);
    if(netpacketfull)
        return NULL;

    level_load(e, m);

    free(oldgs.sectorinfos);
    free(newgs.sectorinfos);
    oldgs.sectorinfos = malloc(nsectors * sizeof(sectorinfo_t));
    newgs.sectorinfos = malloc(nsectors * sizeof(sectorinfo_t));
    for(i=0; i<nsectors; i++)
    {
        oldgs.sectorinfos[i].floorheight = sectors[i].floorheight;
        oldgs.sectorinfos[i].ceilheight = sectors[i].ceilheight;
        newgs.sectorinfos[i].floorheight = sectors[i].floorheight;
        newgs.sectorinfos[i].ceilheight = sectors[i].ceilheight;
    }

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

    serverconn.edict = -1;

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
    stbar_init();

    memset(&player, 0, sizeof(player));
    player.dumb = true;

    player.mobj = NULL;

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
        case SVC_PLAYERDELTAS:
            if(!(curpos = recvplayerdeltas(buf, curpos, len)))
                return;
            break;
        case SVC_SOUND:
        {
            uint8_t sfxid, flags;

            sfxid = net_readu8(buf, curpos++, len);
            flags = net_readu8(buf, curpos++, len);
            if(netpacketfull)
                return;
            if(flags & SND_HASEDICT)
            {
                int edict = net_readu16(buf, curpos, len);
                curpos += 2;
                if(netpacketfull)
                    return;
                if(edict != serverconn.edict)
                    snd_playsoundedict(sfxid, edict);
                else
                    puts("soudn canceled");
            }
            else if(flags & SND_HASPOS)
            {
                float x = net_readfloat(buf, curpos, len); curpos += 4;
                float y = net_readfloat(buf, curpos, len); curpos += 4;
                if(netpacketfull)
                    return;
                snd_playsoundpos(sfxid, x, y);
            }
            break;
        }
        case SVC_SETPLAYEDICT:
            if(!(curpos = recvsetplayedict(buf, curpos, len)))
                return;
            break;
        default:
            fprintf(stderr, "bad packet id %d from server\n", packid);
            return;
        }
    }
}

void recvfromserver(float curtime)
{
    uint8_t buf[NET_MAX_PACKET_SIZE];
    int len;
    bool received;
    sectorinfo_t *oldsectors;

    received = false;
    while((len = net_recv(buf, sizeof(buf), NULL)) > 0)
    {
        if(!received)
        {
            oldsectors = oldgs.sectorinfos;
            memcpy(&oldgs, &newgs, sizeof(gamestate_t));
            oldgs.sectorinfos = oldsectors;
            if(oldsectors && newgs.sectorinfos)
                memcpy(oldsectors, newgs.sectorinfos, nsectors * sizeof(sectorinfo_t));
            received = true;
        }
        lastrecvtime = curtime;
        recvpacket(buf, len);
    }
}

void buildunreliable(netbuf_t* buf)
{
    if(!sendinputs)
        return;
    netbuf_writeu8(buf, CSV_INPUT);
    netbuf_writeu8(buf, inputcmd.flags);
    netbuf_writeu32(buf, inputcmd.angle);
    netbuf_writeu8(buf, inputcmd.switchwpn);
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

EMSCRIPTEN_KEEPALIVE
void net_keepalive(void)
{
    recvfromserver(emscripten_get_now() / 1000.0);
    sendinputs = false;
    sendtoserver();
}
