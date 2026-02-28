#ifndef _PREDICT_H
#define _PREDICT_H

#include "level.h"
#include "player.h"

#define PRED_WINDOW 64

extern playercmd_t inputwindow[PRED_WINDOW];
extern object_t predplayer;

void predictplayer(void);

#endif