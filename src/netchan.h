#ifndef _NETCHAN_H
#define _NETCHAN_H

#include <stdbool.h>
#include <stdint.h>

#include "net.h"

#define MAX_PACKET 1400

typedef struct
{
    int32_t outseq;
    int32_t lastseen;
    int32_t lastsentreliable;

    int32_t inseq;
    int32_t inack;
    bool hadreliable;
    bool inackreliable;


    int32_t msgsize;
    uint8_t msg[MAX_PACKET];

    int32_t reliablesize;
    uint8_t reliable[MAX_PACKET];
} netchan_t;

// returns the location in data[] after reading the header (where the data starts)
void* netchan_recv(netchan_t* state, void* data, int datalen);
void netchan_send(netchan_t* state, int dc, const netbuf_t* unreliable);

#endif