#ifndef _SERVER_SND_H
#define _SERVER_SND_H

#include "../net.h"
#include "../packets.h"
#include "../server/client.h"

void snd_queueedict(int sfxid, int edict);
void snd_queuepos(int sfxid, float x, float y);
void snd_addtobuf(client_t *cl, netbuf_t *buf);
void snd_clearevents(void);

#endif
