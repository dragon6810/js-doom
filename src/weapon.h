#ifndef _WEAPON_H
#define _WEAPON_H

#include <stdbool.h>

#include "info.h"

typedef enum
{
    WEAPON_FIST=0,
    WEAPON_PIST,
    WEAPON_SHOT,
    WEAPON_CHAIN,
    WEAPON_ROCKET,
    WEAPON_PLASMA,
    WEAPON_BFG,
    WEAPON_SAW,
    
    NUM_WEAPONS,

    WEAPON_NONE,
} weapon_e;

typedef enum
{
    AMMO_BUL=0,
    AMMO_SHEL,
    AMMO_ROCK,
    AMMO_CELL,
    NUM_AMMO,

    AMMO_NONE,
} ammo_e;

typedef struct
{
    ammo_e ammo;
    statenum_t upst, downst;
    statenum_t readyst;
    statenum_t firest;
} wpndef_t;

typedef struct
{
    statenum_t state;
    weapon_e cur, pend;
    float time;
} wpnst_t;

extern wpndef_t wpndefs[NUM_WEAPONS];

// set this before ticking
extern struct player_s *curwpnplayer;
extern bool refiring;

void weapon_initstate(wpnst_t* state);
void weapon_docmd(wpnst_t* state, int presses, int switchwpn);
void weapon_tickstate(wpnst_t* state, float ft);
void weapon_dropweapon(wpnst_t* state);
// curwpnplayer must be set
bool weapon_enoughammo(wpnst_t* state);

#endif