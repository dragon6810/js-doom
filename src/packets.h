#ifndef _PACKETS_H
#define _PACKETS_H

#define CVS_HANDSHAKE 0
#define SVC_HANDSHAKE 1 // int32_t player id, int16_t nwads, char[9][nwads] wadnames (in order)
#define SVC_CHANGELEVEL 2 // int8_t episode, int8_t map

#endif