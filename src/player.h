#ifndef _PLAYER_H
#define _PLAYER_H

#include "doommath.h"
#include "level.h"

#define CMD_FORWARD 0x01
#define CMD_BACK 0x02
#define CMD_LEFT 0x04
#define CMD_RIGHT 0x08

typedef struct
{
    object_t *mobj;
    float xvel, yvel, zvel;
} player_t;

typedef struct
{
    uint8_t flags; // CMD_XX
    angle_t angle;
    float frametime;
} playercmd_t;

extern player_t player;

extern float pviewheight, deltaviewheight;

void player_init(void);
void player_docmd(object_t* playobj, const playercmd_t* cmd);
float player_getviewheight(object_t* playobj, float time, float frametime);
void player_use(player_t* player);

#endif