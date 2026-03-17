#include "move.h"

#include "player.h"
#include "snd.h"

void move(object_t* mobj, float ft)
{
    move_xy(mobj, ft);
    move_z(mobj, ft);
}

bool collided;
object_t *movemobj;

angle_t slideangle;
float disttowall;

static void move_explodemissile(void)
{
    movemobj->info.flags &= ~MF_MISSILE;
    level_setmobjstate(movemobj, mobjinfo[movemobj->info.type].deathstate);
    if(mobjinfo[movemobj->info.type].deathsound)
        snd_queueedict(mobjinfo[movemobj->info.type].deathsound, movemobj - mobjs);
}

static void move_validposline(linedef_t* line)
{
    float lower, upper;

    if(collided)
        return;

    if(!line->front && !line->back)
        return;

    if((line->flags & LINEDEF_BLOCKALL)
    || (!movemobj->player && (line->flags & LINEDEF_BLOCKMONSTERS))
    || !line->front || !line->back)
    {
        collided = true;
        return;
    }

    lower = level_linelower(line);
    upper = level_lineupper(line);

    if(upper - lower < movemobj->info.height
    || upper - movemobj->info.z < movemobj->info.height
    || lower - movemobj->info.z > 24)
    {
        collided = true;
        return;
    }
}

static void move_validposmobj(object_t* mobj)
{
    if(collided)
        return;

    if(!(mobj->info.flags & MF_SOLID))
        return;
    
    if(mobj == movemobj)
        return;

    if(movemobj->info.flags & MF_MISSILE && mobj == movemobj->target)
        return;

    collided = true;
}

static bool move_validpos(object_t* mobj, float x, float y)
{
    collided = false;
    level_thingcollisions(x, y, mobjinfo[mobj->info.type].radius, move_validposline, move_validposmobj);
    return !collided;
}

static bool player_slidecollider(float x1, float y1, float x2, float y2, linedef_t* line, float t)
{
    float ldx, ldy;
    float floorheight, ceilheight;

    if(line->flags & LINEDEF_BLOCKALL)
        goto hit;

    if(!line->front && !line->back)
        return false;

    floorheight = level_linelower(line);
    ceilheight = level_lineupper(line);

    if(floorheight - movemobj->info.z > 24)
        goto hit;

    if(ceilheight - floorheight < movemobj->info.height)
        goto hit;

    if(ceilheight - movemobj->info.z < movemobj->info.height)
        goto hit;

    return false;

hit:
    ldx = line->v2->x - line->v1->x;
    ldy = line->v2->y - line->v1->y;
    slideangle = ANGATAN2(ldy, ldx);
    disttowall = t;

    return true;
}

static void move_slide(object_t* mobj, float ft)
{
    int trycount;
    float leadx, leady, trailx, traily;
    float bestdist;
    float velx, vely, x, y, tryx, tryy;
    float tox, toy, remx, remy, projx, projy;
    float linex, liney, dot;
    angle_t bestslideang;
    bool hitanything;

    velx = mobj->info.xvel * ft;
    vely = mobj->info.yvel * ft;
    x = mobj->info.x;
    y = mobj->info.y;

    trycount = 0;
attempt:
    if(++trycount >= 3)
        goto tryaxis;

    leadx = mobj->info.x;
    trailx = mobj->info.x;
    if(mobj->info.xvel > 0)
    {
        leadx += mobjinfo[mobj->info.type].radius;
        trailx -= mobjinfo[mobj->info.type].radius;
    }
    else
    {
        leadx -= mobjinfo[mobj->info.type].radius;
        trailx += mobjinfo[mobj->info.type].radius;
    }
    leady = mobj->info.y;
    traily = mobj->info.y;
    if(mobj->info.yvel > 0)
    {
        leady += mobjinfo[mobj->info.type].radius;
        traily -= mobjinfo[mobj->info.type].radius;
    }
    else
    {
        leady -= mobjinfo[mobj->info.type].radius;
        traily += mobjinfo[mobj->info.type].radius;
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
    if(move_validpos(mobj, tryx, tryy))
        goto moved;
    goto attempt;

tryaxis:
    tryx = x + velx;
    tryy = y;
    if(move_validpos(mobj, tryx, tryy))
    {
        level_unplacemobj(mobj);
        mobj->info.x = tryx;
        mobj->info.y = tryy;
        level_placemobj(mobj);
        return;
    }

    tryx = x;
    tryy = y + vely;
    if(move_validpos(mobj, tryx, tryy))
    {
        level_unplacemobj(mobj);
        mobj->info.x = tryx;
        mobj->info.y = tryy;
        level_placemobj(mobj);
        return;
    }

    tryx = x;
    tryy = y;
moved:
    level_unplacemobj(mobj);
    mobj->info.x = tryx;
    mobj->info.y = tryy;
    level_placemobj(mobj);
    mobj->info.xvel = projx / ft;
    mobj->info.yvel = projy / ft;
}

void move_xy(object_t* mobj, float ft)
{
    float remx, remy, tryx, tryy;
    float framefric;
    bool split;

    movemobj = mobj;

    mobj->info.xvel = CLAMP(mobj->info.xvel, -30 * 35, 30 * 35);
    mobj->info.yvel = CLAMP(mobj->info.yvel, -30 * 35, 30 * 35);

    remx = mobj->info.xvel * ft;
    remy = mobj->info.yvel * ft;
    do
    {
        // this is purely to recreate wallsliding
        // it doesn't really serve its intended purpose
        // because its independent of ft
        if(remx / ft > 15 * 35 || remy / ft > 15 * 35)
        {
            remx *= 0.5;
            remy *= 0.5;
            tryx = mobj->info.x + remx;
            tryy = mobj->info.y + remy;
            split = true;
        }
        else
        {
            tryx = mobj->info.x + remx;
            tryy = mobj->info.y + remy;
            remx = remy = 0;
            split = false;
        }

        if(move_validpos(mobj, tryx, tryy))
        {
            level_unplacemobj(mobj);
            mobj->info.x = tryx;
            mobj->info.y = tryy;
            level_placemobj(mobj);
        }
        else
        {
            if(mobj->player)
                move_slide(mobj, ft);
            else
            {
                mobj->info.xvel = mobj->info.yvel = 0;
                if(mobj->info.flags & MF_MISSILE)
                    move_explodemissile();
                break;
            }
        }
    } while(split);

    level_mobjheights(mobj);
    if(mobj->info.z <= mobjfloorheight)
    {
        framefric = powf(0.90625, 35.0f * ft);
        mobj->info.xvel *= framefric;
        mobj->info.yvel *= framefric;
    }

    if(INRANGE(mobj->info.xvel, -0.0625 * 35.0, 0.0625 * 35.0)
    && INRANGE(mobj->info.yvel, -0.0625 * 35.0, 0.0625 * 35.0))
    {
        mobj->info.xvel = mobj->info.yvel = 0;
        if(mobj->player && INRANGE(mobj->info.state, S_PLAY_RUN1, S_PLAY_RUN4))
            level_setmobjstate(mobj, S_PLAY);
    }
    
    if(mobj->player
    && mobj->info.state == S_PLAY
    && mobj->player->lastcmd.flags & 0xF)
        level_setmobjstate(mobj, S_PLAY_RUN1);
}

void move_z(object_t* mobj, float ft)
{
    movemobj = mobj;

    level_mobjheights(mobj);

    if(!(mobj->info.flags & MF_NOGRAVITY))
        mobj->info.zvel -= 1225 * ft;
    
    mobj->info.z += mobj->info.zvel * ft;
    
    if(mobj->info.z <= mobjfloorheight)
    {
        if(mobj->info.flags & MF_MISSILE)
            move_explodemissile();

        if(mobj->player
        && mobj->info.zvel < -1225.0 * 8.0 / 35.0)
            deltaviewheight = mobj->info.zvel / 35.0 / 8.0;

        mobj->info.z = mobjfloorheight;
        mobj->info.zvel = 0;
    }
    else if(mobj->info.z + mobj->info.height >= mobjceilheight)
    {
        if(mobj->info.flags & MF_MISSILE)
            move_explodemissile();
        mobj->info.z = mobjceilheight - mobj->info.height;
        mobj->info.zvel = 0;
    }
}