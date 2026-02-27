#include "netchan.h"

#include <stdlib.h>

#include "net.h"

void* netchan_recv(netchan_t* state, void* data, int datalen)
{
    int32_t seq, ack;
    bool reliable;

    void *curpos;

    curpos = data;
    seq = net_readi32(data, curpos, datalen);
    curpos += 4;
    ack = net_readi32(data, curpos, datalen);
    curpos += 4;

    // skip qport
    curpos += 2;

    if(netpacketfull)
        return NULL;
    
    reliable = seq >> 31;
    seq &= 0x7FFFFFFF;

    // we already have gotten a newer packet
    if(seq <= state->lastseen)
        return NULL;

    state->inackreliable = ack >> 31;
    state->inack = ack & 0x7FFFFFFF;
    
    state->lastseen = seq;
    state->hadreliable = reliable;
    
    return curpos;
}