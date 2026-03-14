#include "snd.h"

#include "client.h"
#include "net.h"
#include "packets.h"

void snd_queueedict(int sfxid, int edict)
{
    int i;

    netbuf_t buf;
    
    netbuf_init(&buf);
    netbuf_writeu8(&buf, SVC_SOUND);
    netbuf_writeu8(&buf, sfxid);
    netbuf_writeu8(&buf, SND_HASEDICT);
    netbuf_writeu16(&buf, edict);

    for(i=0; i<MAX_CLIENT; i++)
        if(clients[i].state == CLSTATE_CONNECTED)
            netchan_queue(&clients[i].chan, &buf);

    netbuf_free(&buf);
}

void snd_queuepos(int sfxid, float x, float y)
{
    int i;

    netbuf_t buf;
    
    netbuf_init(&buf);
    netbuf_writeu8(&buf, SVC_SOUND);
    netbuf_writeu8(&buf, sfxid);
    netbuf_writeu8(&buf, SND_HASPOS);
    netbuf_writefloat(&buf, x);
    netbuf_writefloat(&buf, y);

    for(i=0; i<MAX_CLIENT; i++)
        if(clients[i].state == CLSTATE_CONNECTED)
            netchan_queue(&clients[i].chan, &buf);

    netbuf_free(&buf);
}
