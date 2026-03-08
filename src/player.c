#include "player.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "special.h"

#define FORWARDTHRUST 50.0
#define SIDETHRUST 40.0
#define TICFRIC 0.90625
#define GRAVITY 1225.0

#define VIEWHEIGHT 41

typedef struct
{
    thinker_t thinker;
    player_t *player;
    float lastdamage;
    bool wasonnukage;
} playerthink_t;

object_t *slideobj;
angle_t slideangle;
float disttowall;

float pviewheight = VIEWHEIGHT, deltaviewheight = 0;
playerinfo_t defaultplayerinfo;

void player_init(void)
{
    pviewheight = VIEWHEIGHT;
    deltaviewheight = 0;

    defaultplayerinfo.flags = PFLAG_BLUCARD | PFLAG_YELCARD | PFLAG_REDCARD;
    defaultplayerinfo.armor = 0;
    defaultplayerinfo.weapons = (1 << WEAPON_FIST) | (1 << WEAPON_PIST);
    defaultplayerinfo.ammo[AMMO_BUL] = 50;
    defaultplayerinfo.ammo[AMMO_SHEL] = 0;
    defaultplayerinfo.ammo[AMMO_ROCK] = 0;
    defaultplayerinfo.ammo[AMMO_CELL] = 0;
}

static bool player_think(playerthink_t* thinker, float frametime, float progtime)
{
    const float damageperiod = 32.0 / 35.0;

    int nukagedamage;

    if(INRANGE(thinker->player->mobj->info.state, S_PLAY, S_PLAY_PAIN2))
    {
        if(!thinker->player->dumb
        && thinker->player->mobj->ssector
        && thinker->player->mobj->info.z <= thinker->player->mobj->ssector->sector->floorheight)
        {
            nukagedamage = 0;
            switch(thinker->player->mobj->ssector->sector->special)
            {
            case 5:
                nukagedamage = 10;
            case 7:
                nukagedamage = 5;
                break;
            case 16:
                nukagedamage = 20;
                break;
            default:
                break;
            }

            if(nukagedamage)
            {
                if(!thinker->wasonnukage)
                    thinker->lastdamage = floorf(progtime / damageperiod) * damageperiod;
                thinker->wasonnukage = true;
            }
            else
                thinker->wasonnukage = false;

            if(nukagedamage && progtime - thinker->lastdamage >= damageperiod)
            {
                level_damagemobj(thinker->player->mobj, nukagedamage);
                thinker->lastdamage = floorf(progtime / damageperiod) * damageperiod;
            }
        }
        else
            thinker->wasonnukage = false;
    }

    return false;
}

static void player_freethink(playerthink_t* thinker)
{
    if(thinker->player && thinker->player->thinker == thinker)
        thinker->player->thinker = NULL;
}

void player_addthinker(player_t* player)
{
    player->thinker = calloc(1, sizeof(playerthink_t));

    player->thinker->func = (thinkfunc_t) player_think;
    player->thinker->freefunc = (thinkfreefunc_t) player_freethink;
    ((playerthink_t*) player->thinker)->player = player;
    ((playerthink_t*) player->thinker)->lastdamage = -32.0 / 35.0;

    addthinker(player->thinker);
}

void player_free(player_t* player)
{
    freethinker(player->thinker);
}

void player_initinfo(playerinfo_t* info)
{
    *info = defaultplayerinfo;
}

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

    if(level_traverseline(leadx, leady, leadx+velx, leady+vely, false, player_slidecollider, NULL))
    {
        hitanything = true;
        if(disttowall < bestdist)
        {
            bestdist = disttowall;
            bestslideang = slideangle;
        }
    }
    if(level_traverseline(trailx, leady, trailx+velx, leady+vely, false, player_slidecollider, NULL))
    {
        hitanything = true;
        if(disttowall < bestdist)
        {
            bestdist = disttowall;
            bestslideang = slideangle;
        }
    }
    if(level_traverseline(leadx, traily, leadx+velx, traily+vely, false, player_slidecollider, NULL))
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

static void player_trymove(object_t* playobj, float frametime, bool airborn)
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

    level_mobjheights(playobj);
    floorz = mobjfloorheight;
    if(playobj->info.z <= floorz)
    {
        if(airborn)
            deltaviewheight = playobj->info.zvel / 35.0 / 8;
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
    float floorz;

    playobj->info.angle = cmd->angle;

    level_mobjheights(playobj);
    floorz = mobjfloorheight;
    if(playobj->info.z <= floorz)
    {
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

        framefric = powf(TICFRIC, 35.0f * cmd->frametime);
        playobj->info.xvel *= framefric;
        playobj->info.yvel *= framefric;
    }

    player_trymove(playobj, cmd->frametime, playobj->info.z > floorz);
}

float player_calcbobamp(object_t* playobj)
{
    float amp;

    amp = playobj->info.xvel * playobj->info.xvel + playobj->info.yvel * playobj->info.yvel;
    amp /= 4.0 * 35.0 * 35.0;
    if(amp > 16)
        amp = 16;

    return amp;
}

float player_calcheadbob(object_t* playobj, float time)
{
    float phase;

    phase = 7 * M_PI_2 * time;

    return 0.5 * sin(phase) * player_calcbobamp(playobj);
}

float player_getviewheight(object_t* playobj, float time, float frametime)
{
    float bob;

    pviewheight += deltaviewheight * frametime * 35.0;
    if(pviewheight >= VIEWHEIGHT)
    {
        pviewheight = VIEWHEIGHT;
        deltaviewheight = 0;
    }
    if(pviewheight < VIEWHEIGHT / 2.0)
    {
        pviewheight = VIEWHEIGHT / 2.0;
        if(deltaviewheight <= 0)
            deltaviewheight = 1;
    }
    if(pviewheight < VIEWHEIGHT)
        deltaviewheight += 0.25 * frametime * 35.0;

    bob = player_calcheadbob(playobj, time);
    return bob + pviewheight + playobj->info.z;
}

static bool usecol(float x1, float y1, float x2, float y2, linedef_t* line, float t)
{
    float top, bottom;
    int side;

    side = level_lineside(line, x1, y1);

    if(!side && line->special)
    {
        switch(line->special)
        {
        case 1:
            special_door(line);
            break;
        };
        
        return true;
    }

    if(!line->front || !line->back)
        return true;

    top = MIN(line->front->sector->ceilheight, line->back->sector->ceilheight);
    bottom = MAX(line->front->sector->floorheight, line->back->sector->floorheight);
    if(bottom >= top)
        return true;

    return false;
}

void player_use(player_t* player)
{
    float x1, y1, x2, y2, dx, dy;

    x1 = player->mobj->info.x;
    y1 = player->mobj->info.y;

    dx = ANGCOS(player->mobj->info.angle) * 64;
    dy = ANGSIN(player->mobj->info.angle) * 64;

    x2 = x1 + dx;
    y2 = y1 + dy;

    level_traverseline(x1, y1, x2, y2, false, usecol, NULL);
}
