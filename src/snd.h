#ifndef _SND_H
#define _SND_H

#include "packets.h"

void snd_queueedict(int sfxid, int edict);
void snd_queuepos(int sfxid, float x, float y);

#endif
