#include "weapon.h"

#include "doommath.h"
#include "player.h"

// 128 pixels, 6 pixels/tic, 35 tics/second
#define RAISETIME (64.0/105.0)
#define LOWERTIME RAISETIME

struct player_s *curwpnplayer = NULL;
bool refiring = false;

wpndef_t wpndefs[NUM_WEAPONS] =
{
    { AMMO_NONE, S_PUNCHUP, S_PUNCHDOWN, S_PUNCH, S_PUNCH1 }, // WEAPON_FIST
    { AMMO_BUL, S_PISTOLUP, S_PISTOLDOWN, S_PISTOL, S_PISTOL1 }, // WEAPON_PIST
    { AMMO_SHEL, S_SGUNUP, S_SGUNDOWN, S_SGUN, S_SGUN1 }, // WEAPON_SHOT
    { AMMO_BUL, S_CHAINUP, S_CHAINDOWN, S_CHAIN, S_CHAIN1 }, // WEAPON_CHAIN
    { AMMO_ROCK, S_MISSILEUP, S_MISSILEDOWN, S_MISSILE, S_MISSILE1 }, // WEAPON_ROCKET
    { AMMO_CELL, S_PLASMAUP, S_PLASMADOWN, S_PLASMA, S_PLASMA1 }, // WEAPON_PLASMA
    { AMMO_CELL, S_BFGUP, S_BFGDOWN, S_BFG, S_BFG1 }, // WEAPON_BFG
    { AMMO_NONE, S_SAWUP, S_SAWDOWN, S_SAW, S_SAW1 }, // WEAPON_SAW
};

void weapon_initstate(wpnst_t* state)
{
    state->cur = WEAPON_PIST;
    state->pend = WEAPON_NONE;
    state->state = wpndefs[WEAPON_PIST].readyst;
    state->time = 0;
}

void weapon_docmd(wpnst_t* state, int presses, int switchwpn)
{
    if(state->cur != switchwpn && INRANGE(switchwpn, 0, NUM_WEAPONS-1))
        state->pend = switchwpn;
    
    if(state->state == wpndefs[state->cur].readyst && presses & CMD_FIRE)
    {
        if(curwpnplayer && curwpnplayer->mobj)
            level_setmobjstate(curwpnplayer->mobj, S_PLAY_ATK1);
        state->state = wpndefs[state->cur].firest;
        state->time = states[state->state].tics / 35.0;
    }
}

void weapon_tickstate(wpnst_t* state, float ft)
{
    wpndef_t *def;
    statenum_t next;

    if(state->cur == WEAPON_NONE)
        return;
    def = &wpndefs[state->cur];

    if(state->state == def->downst)
    {
        state->time -= ft;
        if(state->time <= 0)
        {
            if(curwpnplayer->mobj && curwpnplayer->mobj->info.health)
            {
                state->cur = state->pend;
                state->pend = WEAPON_NONE;
                state->state = wpndefs[state->cur].upst;
                state->time = RAISETIME;
            }
            else
            {
                state->time = 0;
            }
        }
        return;
    }

    if(state->state == def->upst)
    {
        state->time -= ft;
        if(state->time <= 0)
        {
            state->state = def->readyst;
            state->time = 0;
        }
        return;
    }

    if(state->state == def->readyst)
    {
        refiring = false;
        if(state->pend != WEAPON_NONE)
        {
            state->state = def->downst;
            state->time = LOWERTIME;
        }
        return;
    }

    // in animation (firing)

    if(curwpnplayer->mobj && !curwpnplayer->mobj->info.health)
    {
        state->state = def->downst;
        state->time = LOWERTIME;
        return;
    }

    state->time -= ft;
    while(state->time < 0)
    {
        next = states[state->state].nextstate;
        if(next == def->readyst || next == S_NULL)
        {
            state->state = def->readyst;
            state->time = 0;
            return;
        }

        state->state = states[state->state].nextstate;
        state->time += states[state->state].tics / 35.0;

        if(states[state->state].action)
            states[state->state].action();
    }
}

void weapon_dropweapon(wpnst_t* state)
{
    state->state = wpndefs[state->cur].downst;
    state->time = LOWERTIME;
}

void A_ReFire()
{
    if(!curwpnplayer)
        return;

    if(!(curwpnplayer->lastcmd.flags & CMD_FIRE))
        return;

    curwpnplayer->info.weapon.state = wpndefs[curwpnplayer->info.weapon.cur].firest;
    curwpnplayer->info.weapon.time = states[curwpnplayer->info.weapon.state].tics / 35.0;
    refiring = true;
    if(states[curwpnplayer->info.weapon.state].action)
        states[curwpnplayer->info.weapon.state].action();
}
