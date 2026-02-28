#ifndef _PACKETS_H
#define _PACKETS_H

#define USERNAME_LEN 16

#define FIELD_EXISTS 0x0001 // uint8_t
#define FIELD_X 0x0002 // float
#define FIELD_Y 0x0004 // float
#define FIELD_Z 0x0008 // float
#define FIELD_ANGLE 0x0010 // uint32_t (angle_t)
#define FIELD_STATE 0x0020 // int16_t
#define FIELD_TYPE 0x0040 // int16_t

typedef enum
{
    CVS_HANDSHAKE=0, // char username[USERNAME_LEN]
    SVC_SERVERFULL, // 
    SVC_HANDSHAKE, // int32_t clientid, int32_t edictid, int16_t nwads, char[13][nwads] wadnames (in order)
    SVC_CHANGELEVEL, // int8_t episode, int8_t map
    SVC_ENTDELTAS, // n times (uint16_t edict, uint16_t fields, <fields>) 0xFFFF
} packet_e;

#endif