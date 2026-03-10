#include "snd.h"

#include "../net.h"
#include "../packets.h"

#define MAX_SNDEVENTS 64

typedef struct
{
    uint8_t sfxid;
    uint8_t flags;
    uint16_t edict;
    float x, y;
} sndevent_t;

static sndevent_t events[MAX_SNDEVENTS];
static int nevents;

void snd_queueedict(int sfxid, int edict)
{
    sndevent_t *ev;

    if(nevents >= MAX_SNDEVENTS)
        return;

    ev = &events[nevents++];
    ev->sfxid = (uint8_t)sfxid;
    ev->flags = SND_HASEDICT;
    ev->edict = (uint16_t)edict;
}

void snd_queuepos(int sfxid, float x, float y)
{
    sndevent_t *ev;

    if(nevents >= MAX_SNDEVENTS)
        return;

    ev = &events[nevents++];
    ev->sfxid = (uint8_t)sfxid;
    ev->flags = SND_HASPOS;
    ev->x = x;
    ev->y = y;
}

void snd_addtobuf(client_t *cl, netbuf_t *buf)
{
    int i;
    int myedict;
    sndevent_t *ev;

    myedict = (cl->player.mobj) ? (int)(cl->player.mobj - mobjs) : -1;

    for(i=0; i<nevents; i++)
    {
        ev = &events[i];

        // skip own-edict weapon sounds — client plays those locally
        if((ev->flags & SND_HASEDICT) && ev->edict == myedict)
            continue;

        netbuf_writeu8(buf, SVC_SOUND);
        netbuf_writeu8(buf, ev->sfxid);
        netbuf_writeu8(buf, ev->flags);
        if(ev->flags & SND_HASEDICT)
            netbuf_writeu16(buf, ev->edict);
        if(ev->flags & SND_HASPOS)
        {
            netbuf_writefloat(buf, ev->x);
            netbuf_writefloat(buf, ev->y);
        }
    }
}

void snd_clearevents(void)
{
    nevents = 0;
}
