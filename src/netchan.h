#ifndef _NETCHAN_H
#define _NETCHAN_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_UNRELIABLE 512
#define MAX_RELIABLE 512

typedef struct
{
    int32_t outseq;
    int32_t lastseen;
    bool sendreliable;

    int32_t inseq;
    int32_t inack;
    bool hadreliable;
    bool inackreliable;

    int32_t reliablestart;

    int32_t reliablesize;
    uint8_t reliable[MAX_RELIABLE];
} netchan_t;

// returns the location in data[] after reading the header (where the data starts)
void* netchan_recv(netchan_t* state, void* data, int datalen);

#endif