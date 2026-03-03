#include "player.h"

#define FORWARDTHRUST 50.0
#define SIDETHRUST 40.0
#define TICFRIC 0.90625
#define GRAVITY 1225.0

object_t *slideobj;
angle_t slideangle;
float disttowall;

static bool player_slidecollider(float x1, float y1, float x2, float y2, linedef_t* line, float t)
{
    float ldx, ldy;
    float floorheight, ceilheight;

    if(line->flags & LINEDEF_BLOCKALL)
        goto hit;

    if(!line->front && !line->back)
        return false;

    if(line->front)
    {
        floorheight = line->front->sector->floorheight;
        ceilheight = line->front->sector->ceilheight;
        if(line->back)
        {
            floorheight = MAX(floorheight, line->back->sector->floorheight);
            ceilheight = MIN(ceilheight, line->back->sector->ceilheight);
        }
    }
    else
    {
        floorheight = line->back->sector->floorheight;
        ceilheight = line->back->sector->ceilheight;
    }

    if(floorheight - slideobj->info.z > 24)
        goto hit;

    if(ceilheight - floorheight < mobjinfo[slideobj->info.type].height)
        goto hit;

    return false;

hit:
    ldx = line->v2->x - line->v1->x;
    ldy = line->v2->y - line->v1->y;
    slideangle = ANGATAN2(ldy, ldx);
    disttowall = t;

    return true;
}

// just copying the way doom does it to get the same feel,
// jank included
static void player_slidemove(object_t* playobj, float frametime)
{
    int trycount;
    float leadx, leady, trailx, traily;
    float bestdist;
    float velx, vely, x, y, tryx, tryy;
    float tox, toy, remx, remy, projx, projy;
    float linex, liney, dot;
    angle_t bestslideang;
    bool hitanything;

    slideobj = playobj;
    velx = playobj->info.xvel * frametime;
    vely = playobj->info.yvel * frametime;
    x = playobj->info.x;
    y = playobj->info.y;

    trycount = 0;
attempt:
    if(++trycount >= 3)
        goto tryaxis;

    leadx = playobj->info.x;
    trailx = playobj->info.x;
    if(playobj->info.xvel > 0)
    {
        leadx += mobjinfo[playobj->info.type].radius;
        trailx -= mobjinfo[playobj->info.type].radius;
    }
    else
    {
        leadx -= mobjinfo[playobj->info.type].radius;
        trailx += mobjinfo[playobj->info.type].radius;
    }
    leady = playobj->info.y;
    traily = playobj->info.y;
    if(playobj->info.yvel > 0)
    {
        leady += mobjinfo[playobj->info.type].radius;
        traily -= mobjinfo[playobj->info.type].radius;
    }
    else
    {
        leady -= mobjinfo[playobj->info.type].radius;
        traily += mobjinfo[playobj->info.type].radius;
    }

    hitanything = false;
    bestdist = INFINITY;
    bestslideang = 0;

    if(level_castsegmentagainstlines(leadx, leady, leadx+velx, leady+vely, player_slidecollider))
    {
        hitanything = true;
        if(disttowall < bestdist)
        {
            bestdist = disttowall;
            bestslideang = slideangle;
        }
    }
    if(level_castsegmentagainstlines(trailx, leady, trailx+velx, leady+vely, player_slidecollider))
    {
        hitanything = true;
        if(disttowall < bestdist)
        {
            bestdist = disttowall;
            bestslideang = slideangle;
        }
    }
    if(level_castsegmentagainstlines(leadx, traily, leadx+velx, traily+vely, player_slidecollider))
    {
        hitanything = true;
        if(disttowall < bestdist)
        {
            bestdist = disttowall;
            bestslideang = slideangle;
        }
    }

    if(!hitanything)
        goto tryaxis;

    bestdist -= 0.01;
    tox = velx * bestdist;
    toy = vely * bestdist;
    remx = velx - tox;
    remy = vely - toy;

    linex = ANGCOS(bestslideang);
    liney = ANGSIN(bestslideang);

    dot = remx * linex + remy * liney;
    projx = linex * dot;
    projy = liney * dot;

    velx = tox + projx;
    vely = toy + projy;
    
    tryx = x + velx;
    tryy = y + vely;
    if(level_validobjpos(playobj, tryx, tryy))
        goto moved;
    goto attempt;

tryaxis:
    tryx = x + velx;
    tryy = y;
    if(level_validobjpos(playobj, tryx, tryy))
        goto moved;
    tryx = x;
    tryy = y + vely;
    if(level_validobjpos(playobj, tryx, tryy))
        goto moved;
    tryx = x;
    tryy = y;

moved:
    playobj->info.x = tryx;
    playobj->info.y = tryy;
    playobj->info.xvel = (tryx - x) / frametime;
    playobj->info.yvel = (tryy - y) / frametime;
}

static void player_trymove(object_t* playobj, float frametime)
{
    float x, y, floorz;

    playobj->info.z += playobj->info.zvel * frametime;

    x = playobj->info.x + playobj->info.xvel * frametime;
    y = playobj->info.y + playobj->info.yvel * frametime;

    if(level_validobjpos(playobj, x, y))
    {
        playobj->info.x = x;
        playobj->info.y = y;
        level_unplacemobj(playobj);
        level_placemobj(playobj);
    }
    else
    {
        player_slidemove(playobj, frametime);
        level_unplacemobj(playobj);
        level_placemobj(playobj);
    }

    floorz = level_mobjfloorheight(playobj);
    if(playobj->info.z <= floorz)
    {
        playobj->info.z = floorz;
        playobj->info.zvel = 0;
    }
    else
        playobj->info.zvel -= GRAVITY * frametime;
}

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

    thrustx = (forwardmove * cosangle - leftmove * sinangle) * 35.0;
    thrusty = (forwardmove * sinangle + leftmove * cosangle) * 35.0;

    playobj->info.xvel += thrustx * cmd->frametime;
    playobj->info.yvel += thrusty * cmd->frametime;

    playobj->info.angle = cmd->angle;

    framefric = powf(TICFRIC, 35.0f * cmd->frametime);
    playobj->info.xvel *= framefric;
    playobj->info.yvel *= framefric;

    player_trymove(playobj, cmd->frametime);
}

float player_calcheadbob(object_t* playobj, float time)
{
    float amp, phase;

    amp = playobj->info.xvel * playobj->info.xvel + playobj->info.yvel * playobj->info.yvel;
    amp /= 4.0 * 35.0 * 35.0;
    if(amp > 16)
        amp = 16;

    phase = 7 * M_PI_2 * time;

    return 0.5 * sin(phase) * amp;
}
