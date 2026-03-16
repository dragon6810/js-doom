#include "player.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "move.h"
#include "snd.h"
#include "special.h"
#include "level.h"

#define FORWARDTHRUST 50.0
#define SIDETHRUST 40.0

#define VIEWHEIGHT 41

typedef struct
{
    thinker_t thinker;
    player_t *player;
    float lastdamage;
    bool wasonnukage;
} playerthink_t;

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

bool player_think(playerthink_t* thinker, float frametime, float progtime)
{
    const float stopspeed = 0.0625 * 35.0;
    const float damageperiod = 32.0 / 35.0;

    int nukagedamage;

    curwpnplayer = thinker->player;

    if(curwpnplayer->mobj && !curwpnplayer->mobj->info.health 
    && curwpnplayer->info.weapon.state != wpndefs[curwpnplayer->info.weapon.cur].downst)
        weapon_dropweapon(&curwpnplayer->info.weapon);

    if(!thinker->player->dumb)
        weapon_tickstate(&thinker->player->info.weapon, frametime);

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
                break;
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
                level_damagemobj(thinker->player->mobj, nukagedamage, NULL);
                thinker->lastdamage = floorf(progtime / damageperiod) * damageperiod;
            }
        }
        else
            thinker->wasonnukage = false;
    }

    if(!thinker->player->lastcmd.flags
    && thinker->player->mobj->info.state == S_PLAY_RUN1
    && INRANGE(thinker->player->mobj->info.xvel, -stopspeed, stopspeed)
    && INRANGE(thinker->player->mobj->info.yvel, -stopspeed, stopspeed))
        level_setmobjstate(thinker->player->mobj, mobjinfo[MT_PLAYER].spawnstate);
    else if((thinker->player->lastcmd.flags & (0xF)) && thinker->player->mobj->info.state == S_PLAY)
        level_setmobjstate(thinker->player->mobj, mobjinfo[MT_PLAYER].seestate);

    return false;
}

bool player_freethink(thinker_t* thinker)
{
    if(((playerthink_t*)thinker)->player && ((playerthink_t*)thinker)->player->thinker == thinker)
        ((playerthink_t*)thinker)->player->thinker = NULL;
    
    return false;
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

static player_t *curplayer;

static bool player_pickupwpn(weapon_e type)
{
    bool gavewpn, gaveammo;

    gavewpn = gaveammo = false;

    if(wpndefs[type].ammo != AMMO_NONE
    && curplayer->info.ammo[wpndefs[type].ammo] < player_maxammo(curplayer, wpndefs[type].ammo))
    {
        gaveammo = true;
        switch(wpndefs[type].ammo)
        {
        case AMMO_BUL:
            curplayer->info.ammo[AMMO_BUL] = MIN(curplayer->info.ammo[AMMO_BUL] + 20, player_maxammo(curplayer, AMMO_BUL));
            break;
        case AMMO_SHEL:
            curplayer->info.ammo[AMMO_SHEL] = MIN(curplayer->info.ammo[AMMO_SHEL] + 8, player_maxammo(curplayer, AMMO_SHEL));
            break;
        case AMMO_ROCK:
            curplayer->info.ammo[AMMO_ROCK] = MIN(curplayer->info.ammo[AMMO_ROCK] + 2, player_maxammo(curplayer, AMMO_ROCK));
            break;
        case AMMO_CELL:
            curplayer->info.ammo[AMMO_CELL] = MIN(curplayer->info.ammo[AMMO_CELL] + 60, player_maxammo(curplayer, AMMO_CELL));
            break;
        default:
            break;
        }
    }

    if(!(curplayer->info.weapons & (1 << type)))
    {
        gavewpn = true;
        curplayer->info.weapons |= 1 << type;
        curplayer->info.weapon.pend = type;
    }

    return gaveammo || gavewpn;
}

bool player_walkovercol(float x1, float y1, float x2, float y2, linedef_t* line, float t)
{
    if(!line->special || !line->tag)
        return false;

    level_trigger(curplayer->mobj, line->tag, line->special);

    return false;
}

void player_pickupcol(object_t* obj)
{
    sound_e sound;

    if(curplayer->mobj->info.z > obj->info.z + obj->info.height)
        return;

    if(curplayer->mobj->info.z + curplayer->mobj->info.height < obj->info.z)
        return;

    sound = sfx_itemup;
    switch(states[obj->info.state].sprite)
    {
    case SPR_ARM1:
        if(curplayer->info.armor >= 100)
            return;
        curplayer->info.flags &= ~PFLAG_BLUARMOR;
        curplayer->info.armor = 100;
        break;
    case SPR_ARM2:
        if(curplayer->info.armor >= 200)
            return;
        curplayer->info.flags |= PFLAG_BLUARMOR;
        curplayer->info.armor = 200;
        break;
    case SPR_BON1:
        curplayer->mobj->info.health++;
        if(curplayer->mobj->info.health > 200)
            curplayer->mobj->info.health = 200;
        break;
    case SPR_BON2:
        curplayer->info.armor++;
        if(curplayer->info.armor > 200)
            curplayer->info.armor = 200;
        break;
    case SPR_STIM:
        if(curplayer->mobj->info.health >= 100)
            return;
        curplayer->mobj->info.health += 10;
        if(curplayer->mobj->info.health > 100)
            curplayer->mobj->info.health = 100;
        break;
    case SPR_MEDI:
        if(curplayer->mobj->info.health >= 100)
            return;
        curplayer->mobj->info.health += 25;
        if(curplayer->mobj->info.health > 100)
            curplayer->mobj->info.health = 100;
        break;
    case SPR_CLIP:
        if(curplayer->info.ammo[AMMO_BUL] >= player_maxammo(curplayer, AMMO_BUL))
            return;
        curplayer->info.ammo[AMMO_BUL] += 10;
        if(curplayer->info.ammo[AMMO_BUL] > player_maxammo(curplayer, AMMO_BUL))
            curplayer->info.ammo[AMMO_BUL] = player_maxammo(curplayer, AMMO_BUL);
        break;
    case SPR_AMMO:
        if(curplayer->info.ammo[AMMO_BUL] >= player_maxammo(curplayer, AMMO_BUL))
            return;
        curplayer->info.ammo[AMMO_BUL] += 50;
        if(curplayer->info.ammo[AMMO_BUL] > player_maxammo(curplayer, AMMO_BUL))
            curplayer->info.ammo[AMMO_BUL] = player_maxammo(curplayer, AMMO_BUL);
        break;
    case SPR_SHEL:
        if(curplayer->info.ammo[AMMO_SHEL] >= player_maxammo(curplayer, AMMO_SHEL))
            return;
        curplayer->info.ammo[AMMO_SHEL] += 4;
        if(curplayer->info.ammo[AMMO_SHEL] > player_maxammo(curplayer, AMMO_SHEL))
            curplayer->info.ammo[AMMO_SHEL] = player_maxammo(curplayer, AMMO_SHEL);
        break;
    case SPR_SBOX:
        if(curplayer->info.ammo[AMMO_SHEL] >= player_maxammo(curplayer, AMMO_SHEL))
            return;
        curplayer->info.ammo[AMMO_SHEL] += 20;
        if(curplayer->info.ammo[AMMO_SHEL] > player_maxammo(curplayer, AMMO_SHEL))
            curplayer->info.ammo[AMMO_SHEL] = player_maxammo(curplayer, AMMO_SHEL);
        break;
    case SPR_ROCK:
        if(curplayer->info.ammo[AMMO_ROCK] >= player_maxammo(curplayer, AMMO_ROCK))
            return;
        curplayer->info.ammo[AMMO_ROCK] += 1;
        if(curplayer->info.ammo[AMMO_ROCK] > player_maxammo(curplayer, AMMO_ROCK))
            curplayer->info.ammo[AMMO_ROCK] = player_maxammo(curplayer, AMMO_ROCK);
        break;
    case SPR_BROK:
        if(curplayer->info.ammo[AMMO_ROCK] >= player_maxammo(curplayer, AMMO_ROCK))
            return;
        curplayer->info.ammo[AMMO_ROCK] += 5;
        if(curplayer->info.ammo[AMMO_ROCK] > player_maxammo(curplayer, AMMO_ROCK))
            curplayer->info.ammo[AMMO_ROCK] = player_maxammo(curplayer, AMMO_ROCK);
        break;
    case SPR_CELL:
        if(curplayer->info.ammo[AMMO_CELL] >= player_maxammo(curplayer, AMMO_CELL))
            return;
        curplayer->info.ammo[AMMO_CELL] += 20;
        if(curplayer->info.ammo[AMMO_CELL] > player_maxammo(curplayer, AMMO_CELL))
            curplayer->info.ammo[AMMO_CELL] = player_maxammo(curplayer, AMMO_CELL);
        break;
    case SPR_CELP:
        if(curplayer->info.ammo[AMMO_CELL] >= player_maxammo(curplayer, AMMO_CELL))
            return;
        curplayer->info.ammo[AMMO_CELL] += 100;
        if(curplayer->info.ammo[AMMO_CELL] > player_maxammo(curplayer, AMMO_CELL))
            curplayer->info.ammo[AMMO_CELL] = player_maxammo(curplayer, AMMO_CELL);
        break;
    case SPR_SHOT:
        if(!player_pickupwpn(WEAPON_SHOT))
            return;
        sound = sfx_wpnup;
        break;
    case SPR_MGUN:
        if(!player_pickupwpn(WEAPON_CHAIN))
            return;
        sound = sfx_wpnup;
        break;
    default:
        return;
    }

    level_removemobj(obj);
    curplayer->pickupcnt += 6;
    snd_queueedict(sound, curplayer->mobj - mobjs);
}

void player_docmd(player_t* play, const playercmd_t* cmd)
{
    float framespeed;
    float leftmove, forwardmove;
    float sinangle, cosangle;
    float thrustx, thrusty;
    float framefric;
    float floorz;
    float x, y;

    if(play->mobj && play->mobj->info.health)
        play->mobj->info.angle = cmd->angle;

    level_mobjheights(play->mobj);
    floorz = mobjfloorheight;
    if(play->mobj->info.z <= floorz && play->mobj && play->mobj->info.health)
    {
        sinangle = ANGSIN(play->mobj->info.angle);
        cosangle = ANGCOS(play->mobj->info.angle);

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

        play->mobj->info.xvel += thrustx * cmd->frametime;
        play->mobj->info.yvel += thrusty * cmd->frametime;
    }

    if(play->mobj && !play->dumb && play->mobj->info.health)
    {
        x = play->mobj->info.x + play->mobj->info.xvel * cmd->frametime;
        y = play->mobj->info.y + play->mobj->info.yvel * cmd->frametime;

        curplayer = play;
        level_thingcollisions(x, y, mobjinfo[MT_PLAYER].radius, NULL, player_pickupcol);
        level_traverseline(play->mobj->info.x, play->mobj->info.y, x, y, true, player_walkovercol, NULL);
    }
    
    move(play->mobj, cmd->frametime);

    curwpnplayer = play;
    weapon_docmd(&play->info.weapon, cmd->flags, cmd->switchwpn);
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

    if(playobj->info.health)
    {
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
    }
    else
    {
        pviewheight -= frametime * 35.0;
        if(pviewheight < 6)
            pviewheight = 6;
        bob = 0;
    }

    return bob + pviewheight + playobj->info.z;
}

object_t *usemobj;

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
        case 26:
        case 27:
        case 28:
        case 31:
        case 32:
        case 33:
        case 34:
            special_door(usemobj, line, line->special);
            break;
        };
        
        return true;
    }

    if(!line->front || !line->back)
    {
        snd_queueedict(sfx_noway, usemobj - mobjs);
        return true;
    }

    top = MIN(line->front->sector->ceilheight, line->back->sector->ceilheight);
    bottom = MAX(line->front->sector->floorheight, line->back->sector->floorheight);
    if(bottom >= top)
    {
        snd_queueedict(sfx_noway, usemobj - mobjs);
        return true;
    }

    return false;
}

int player_maxammo(player_t* player, ammo_e ammo)
{
    int multiplier;

    multiplier = 1;
    if(player->info.flags & PFLAG_BACKPACK)
        multiplier = 2;

    switch(ammo)
    {
    case AMMO_BUL:
        return 200 * multiplier;
    case AMMO_SHEL:
        return 50 * multiplier;
    case AMMO_ROCK:
        return 50 * multiplier;
    case AMMO_CELL:
        return 300 * multiplier;
    default:
        return 0;
    }
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

    usemobj = player->mobj;

    level_traverseline(x1, y1, x2, y2, false, usecol, NULL);
}
