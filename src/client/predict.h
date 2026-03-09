#ifndef _PREDICT_H
#define _PREDICT_H

#include <stdint.h>

#include "level.h"
#include "packets.h"
#include "player.h"

#define PRED_WINDOW 256

extern playercmd_t inputwindow[PRED_WINDOW];
extern gamestate_t oldgs;
extern gamestate_t newgs;
extern wpnst_t startwpn;

void predictplayer(void);
void interpsectors(float abstime);
void interpentities(float abstime);

#endif