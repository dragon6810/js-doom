#ifndef _PLAYER_H
#define _PLAYER_H

#include "doommath.h"
#include "level.h"
#include "think.h"

#define CMD_FORWARD 0x01
#define CMD_BACK 0x02
#define CMD_LEFT 0x04
#define CMD_RIGHT 0x08

#define PFLAG_BLUARMOR 0x01
#define PFLAG_BACKPACK 0x02
#define PFLAG_BLUCARD 0x04
#define PFLAG_YELCARD 0x08
#define PFLAG_REDCARD 0x10
#define PFLAG_BLUSKUL 0x20
#define PFLAG_YELSKUL 0x40
#define PFLAG_REDSKUL 0x80

typedef enum
{
    WEAPON_NONE=0,

    WEAPON_FIST,
    WEAPON_PIST,
    WEAPON_SHOT,
    WEAPON_CHAIN,
    WEAPON_ROCKET,
    WEAPON_PLASMA,
    WEAPON_SAW,
    NUM_WEAPONS,
} weapon_e;

typedef enum
{
    AMMO_BUL=0,
    AMMO_SHEL,
    AMMO_ROCK,
    AMMO_CELL,
    NUM_AMMO,
} ammo_e;

typedef struct
{
    int flags;
    int armor;
    int weapons; // each bit is a flag (bit is 1 << (weapon))
    int ammo[NUM_AMMO];
    int frags;
} playerinfo_t;

typedef struct
{
    uint8_t flags; // CMD_XX
    angle_t angle;
    float frametime;
} playercmd_t;

typedef struct
{
    object_t *mobj;
    playerinfo_t info;
    thinker_t *thinker;
    playercmd_t lastcmd;

    bool dumb; // 'dumb' player, true for client, false for server
} player_t;

extern player_t player;

extern float pviewheight, deltaviewheight;
extern playerinfo_t defaultplayerinfo;

void player_init(void);
void player_addthinker(player_t* player);
void player_free(player_t* player);
// default spawn values
void player_initinfo(playerinfo_t* info);
void player_docmd(object_t* playobj, const playercmd_t* cmd);
float player_calcbobamp(object_t* playobj);
float player_calcheadbob(object_t* playobj, float time);
float player_getviewheight(object_t* playobj, float time, float frametime);
void player_use(player_t* player);

#endif