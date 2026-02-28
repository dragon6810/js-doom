#include "player.h"

#define FORWARDTHRUST 50.0
#define SIDETHRUST 40.0
#define TICFRIC 0.90625

void player_docmd(object_t* playobj, const playercmd_t* cmd)
{
    float framespeed;
    float leftmove, forwardmove;
    float sinangle, cosangle;
    float thrustx, thrusty;
    float framefric;

    sinangle = ANGSIN(playobj->info.angle);
    cosangle = ANGCOS(playobj->info.angle);

    leftmove = forwardmove = 0;
    if(cmd->flags & CMD_FORWARD)
        forwardmove += FORWARDTHRUST;
    if(cmd->flags & CMD_BACK)
        forwardmove -= FORWARDTHRUST;
    if(cmd->flags & CMD_LEFT)
        leftmove += SIDETHRUST;
    if(cmd->flags & CMD_RIGHT)
        leftmove -= SIDETHRUST;

    thrustx = forwardmove * cosangle - leftmove * sinangle;
    thrusty = forwardmove * sinangle + leftmove * cosangle;

    playobj->info.xvel += thrustx * cmd->frametime;
    playobj->info.yvel += thrusty * cmd->frametime;

    playobj->info.x += playobj->info.xvel;
    playobj->info.y += playobj->info.yvel;

    playobj->info.angle = cmd->angle;

    framefric = powf(TICFRIC, 35.0f * cmd->frametime);
    playobj->info.xvel *= framefric;
    playobj->info.yvel *= framefric;

    level_unplacemobj(playobj);
    level_placemobj(playobj);
    playobj->info.z = playobj->ssector->sector->floorheight;
}