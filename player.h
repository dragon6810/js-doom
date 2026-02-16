#include "math.h"

#define CMD_FORWARD 0x01
#define CMD_BACK 0x02
#define CMD_LEFT 0x04
#define CMD_RIGHT 0x08

typedef struct
{
    float x, y, z;
    angle_t angle;
    float xvel, yvel, zvel;
} player_t;

typedef struct
{
    uint8_t flags; // CMD_XX
    angle_t deltaangle;
    float frametime;
} playercmd_t;

extern player_t player;

void player_docmd(player_t* player, const playercmd_t* cmd);