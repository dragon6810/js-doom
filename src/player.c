#include "player.h"

void player_docmd(player_t* player, const playercmd_t* cmd)
{
    const float movespeed = 583;

    float framespeed;
    float leftmove, forwardmove;
    float sinangle, cosangle;

    framespeed = movespeed * cmd->frametime;

    sinangle = ANGSIN(player->mobj->info.angle);
    cosangle = ANGCOS(player->mobj->info.angle);

    leftmove = forwardmove = 0;
    if(cmd->flags & CMD_FORWARD)
        forwardmove += framespeed;
    if(cmd->flags & CMD_BACK)
        forwardmove -= framespeed;
    if(cmd->flags & CMD_LEFT)
        leftmove += framespeed;
    if(cmd->flags & CMD_RIGHT)
        leftmove -= framespeed;

    player->mobj->info.x += forwardmove * cosangle - leftmove * sinangle;
    player->mobj->info.y += forwardmove * sinangle + leftmove * cosangle;

    player->mobj->info.angle = cmd->angle;
}