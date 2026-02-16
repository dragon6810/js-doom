#include "player.h"

player_t player = {};

void player_docmd(player_t* player, const playercmd_t* cmd)
{
    const float movespeed = 583;

    float framespeed;
    float leftmove, forwardmove;
    float sinangle, cosangle;

    framespeed = movespeed * cmd->frametime;

    sinangle = ANGSIN(player->angle);
    cosangle = ANGCOS(player->angle);

    leftmove = forwardmove = 0;
    if(cmd->flags & CMD_FORWARD)
        forwardmove += framespeed;
    if(cmd->flags & CMD_BACK)
        forwardmove -= framespeed;
    if(cmd->flags & CMD_LEFT)
        leftmove += framespeed;
    if(cmd->flags & CMD_RIGHT)
        leftmove -= framespeed;

    player->x += forwardmove * cosangle - leftmove * sinangle;
    player->y += forwardmove * sinangle + leftmove * cosangle;
}