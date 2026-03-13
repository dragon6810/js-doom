#ifndef _PACKETS_H
#define _PACKETS_H

#include "level.h"

#define USERNAME_LEN 16

#define FIELD_EXISTS 0x0001 // uint8_t
#define FIELD_X 0x0002 // float
#define FIELD_Y 0x0004 // float
#define FIELD_Z 0x0008 // float
#define FIELD_ANGLE 0x0010 // uint32_t (angle_t)
#define FIELD_STATE 0x0020 // int16_t
#define FIELD_TYPE 0x0040 // int16_t
#define FIELD_XVEL 0x0080 // float
#define FIELD_YVEL 0x0100 // float
#define FIELD_ZVEL 0x0200 // float
#define FIELD_COLOR 0x0400 // uint8_t
#define FIELD_HEALTH 0x0800 // int16_t
#define FIELD_FLAGS 0x1000 // int32_t
#define FIELD_HEIGHT 0x2000 // int16_t

#define SFIELD_FLOOR 0x01 // float
#define SFIELD_CEIL  0x02 // float

#define PFIELD_FLAGS 0x0001 // uint8_t
#define PFIELD_ARMOR 0x0002 // uint16_t
#define PFIELD_WEAPONS 0x0004 // uint8_t
#define PFIELD_BULLETS 0x0008 // uint16_t
#define PFIELD_SHELLS 0x0010 // uint16_t
#define PFIELD_ROCKETS 0x0020 // uint16_t
#define PFIELD_CELLS 0x0040 // uint16_t
#define PFIELD_FRAGS 0x0080 // uint16_t
#define PFIELD_WPNST 0x0100 // int16_t
#define PFIELD_CURWPN 0x0200 // uint8_t (weapon_e)
#define PFIELD_PENDWPN 0x0400 // uint8_t (weapon_e)
#define PFIELD_WPNTIME 0x0800 // float

#define SND_HASEDICT 0x01 // uint16_t edict
#define SND_HASPOS   0x02 // float x, float y

typedef enum
{
    SFX_PISTOL=0,
    SFX_SHOTGN,
    SFX_DOROPN,
    SFX_DORCLS,
    NUM_SFX,
} sfxid_e;

typedef struct
{
    float floorheight;
    float ceilheight;
} sectorinfo_t;

typedef enum
{
    CVS_HANDSHAKE=0, // char username[USERNAME_LEN]
    SVC_SERVERFULL, //
    SVC_HANDSHAKE, // int32_t clientid, int16_t nwads, char[13][nwads] wadnames (in order)
    SVC_CHANGELEVEL, // int8_t episode, int8_t map
    SVC_ENTDELTAS, // n times (uint16_t edict, uint16_t fields, <fields>) 0xFFFF, n times (uint16_t sector, uint8_t fields, <fields>) 0xFFFF
    CSV_INPUT, // uint8_t flags, uint32_t (angle_t) angle, float frametime
    CSV_USE, //
    SVC_PLAYERDELTAS, // uint16_t fields, <fields>
    SVC_SOUND, // uint8_t sfxid, uint8_t flags, [uint16_t edict] OR [float x, float y]
    SVC_SETPLAYEDICT, // int32_t newedict
    CSV_RESPAWN, // 
    SVC_PICKUP, // (use purely for sound and fade, doesn't say what you got)
} packet_e;

typedef struct
{
    int maxmobj;
    objinfo_t mobjs[MAX_MOBJ];
    sectorinfo_t *sectorinfos;
} gamestate_t;

#endif