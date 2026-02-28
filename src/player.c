#include "player.h"

void player_docmd(object_t* playobj, const playercmd_t* cmd)
{
    const float movespeed = 583;

    float framespeed;
    float leftmove, forwardmove;
    float sinangle, cosangle;

    framespeed = movespeed * cmd->frametime;

    sinangle = ANGSIN(playobj->info.angle);
    cosangle = ANGCOS(playobj->info.angle);

    leftmove = forwardmove = 0;
    if(cmd->flags & CMD_FORWARD)
        forwardmove += framespeed;
    if(cmd->flags & CMD_BACK)
        forwardmove -= framespeed;
    if(cmd->flags & CMD_LEFT)
        leftmove += framespeed;
    if(cmd->flags & CMD_RIGHT)
        leftmove -= framespeed;

    playobj->info.x += forwardmove * cosangle - leftmove * sinangle;
    playobj->info.y += forwardmove * sinangle + leftmove * cosangle;

    playobj->info.angle = cmd->angle;

    level_unplacemobj(playobj);
    level_placemobj(playobj);
    playobj->info.z = playobj->ssector->sector->floorheight;
}