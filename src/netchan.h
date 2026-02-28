#ifndef _NETCHAN_H
#define _NETCHAN_H

#include <stdbool.h>
#include <stdint.h>

#include "net.h"

#define MAX_PACKET 8192
#define MAX_MSGQUE 64

typedef struct
{
    int32_t outseq;
    int32_t lastseen;
    int32_t lastsentreliable;
    
    int32_t inack;
    bool hadreliable;
    bool inackreliable;

    // toggles with each new outgoing reliable message
    bool outreliable;
    // tracks the reliable bit of the last reliable we processed
    bool inreliable;

    // on the last recv, a new (not retransmitted) reliable was received
    bool gotnewreliable;
    // on the last recv, a previously unacknowledged reliable was acknowledged
    bool justgotack;

    bool msgoverflow;

    int32_t nmsg;
    int32_t msgsizes[MAX_MSGQUE];
    uint8_t msg[MAX_MSGQUE][MAX_PACKET];

    int32_t reliablesize;
    uint8_t reliable[MAX_PACKET];
} netchan_t;

// returns the location in data[] after reading the header (where the data starts)
void* netchan_recv(netchan_t* state, void* data, int datalen);
bool netchan_send(netchan_t* state, int dc, const netbuf_t* unreliable);
bool netchan_queue(netchan_t* state, const netbuf_t* msg);

#endif