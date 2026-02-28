#ifndef _PACKETS_H
#define _PACKETS_H

#define USERNAME_LEN 16

typedef enum
{
    CVS_HANDSHAKE=0, // char username[USERNAME_LEN]
    SVC_SERVERFULL, // 
    SVC_HANDSHAKE, // int32_t player id, int16_t nwads, char[13][nwads] wadnames (in order)
    SVC_CHANGELEVEL, // int8_t episode, int8_t map
} packet_e;

#endif