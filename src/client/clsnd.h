#ifndef _CLIENT_SND_H
#define _CLIENT_SND_H

#include "doommath.h"
#include "packets.h"

void snd_init(void);
void snd_playsoundedict(int sfxid, int edict);
void snd_playsoundpos(int sfxid, float x, float y);
void snd_update(float lx, float ly, angle_t lang);

// stubs satisfying the linker for shared code that calls these server-side
void snd_queueedict(int sfxid, int edict);
void snd_queuepos(int sfxid, float x, float y);

#endif
