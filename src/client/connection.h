#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <stdint.h>

#include "netchan.h"
#include "packets.h"

typedef enum
{
    CLSTATE_DC=0,
    CLSTATE_SHAKING,
    CLSTATE_CONNECTED,
} clstate_e;

typedef struct
{
    netchan_t chan;
    clstate_e state;
    int32_t clientid;
    int32_t edict;
    double shaketimer;
    double nextattempt;
} conn_t;

extern conn_t serverconn;

void recvfromserver(void);
void sendtoserver(void);

#endif
