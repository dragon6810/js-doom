#include "netchan.h"

#include <stdlib.h>
#include <string.h>

#include "net.h"

static void netchan_trytransfermsg(netchan_t* state)
{
    if(!state->reliablesize && state->nmsg && state->msgsizes[0])
    {
        state->reliablesize = state->msgsizes[0];
        memcpy(state->reliable, state->msg[0], state->reliablesize);
        state->nmsg--;

        memmove(&state->msg[0], &state->msg[1], state->nmsg * MAX_PACKET);
    }
}

void* netchan_recv(netchan_t* state, void* data, int datalen)
{
    int32_t seq, ack;
    bool reliable, whichreliable;

    void *curpos;

    state->justgotack = false;
    state->gotnewreliable = false;

    curpos = data;
    seq = net_readi32(data, curpos, datalen);
    curpos += 4;
    ack = net_readi32(data, curpos, datalen);
    curpos += 4;

    // skip qport
    curpos += 2;

    if(netpacketfull)
        return NULL;

    reliable = ((uint32_t)seq >> 31) & 1;
    whichreliable = ((uint32_t)seq >> 30) & 1;
    seq &= 0x3FFFFFFF;

    // we already got a newer packet
    if(seq <= state->lastseen)
        return NULL;

    state->inackreliable = ack >> 31;
    state->inack = ack & 0x7FFFFFFF;

    state->lastseen = seq;
    state->hadreliable = reliable;

    if(state->inack >= state->lastsentreliable && state->lastsentreliable)
    {
        state->justgotack = true;
        state->reliablesize = state->lastsentreliable = 0;
    }

    netchan_trytransfermsg(state);

    if(reliable)
    {
        // same toggle bit as last time = retransmit, drop payload
        if(whichreliable == state->inreliable)
            return NULL;

        state->gotnewreliable = true;
        state->inreliable = whichreliable;
    }

    return curpos;
}

void netchan_send(netchan_t* state, int dc, const netbuf_t* unreliable)
{
    netbuf_t buf;
    uint32_t seq, ack;
    bool sendreliable;

    netchan_trytransfermsg(state);

    sendreliable = state->reliablesize > 0;

    // toggle the which-reliable bit only when first sending a new reliable
    if(sendreliable && !state->lastsentreliable)
        state->outreliable = !state->outreliable;

    state->outseq++;
    seq = (uint32_t) state->outseq;
    if(sendreliable)
    {
        seq |= 0x80000000U;
        if(state->outreliable) seq |= 0x40000000U;
    }

    ack = (uint32_t) state->lastseen;
    if(state->hadreliable)
        ack |= 0x80000000U;

    netbuf_init(&buf);
    netbuf_writeu32(&buf, seq);
    netbuf_writeu32(&buf, ack);
    netbuf_writei16(&buf, 0);

    if(sendreliable)
        netbuf_writedata(&buf, state->reliable, state->reliablesize);

    if(unreliable && unreliable->len > 0)
        netbuf_writedata(&buf, unreliable->data, unreliable->len);

    net_send(dc, buf.data, buf.len);
    netbuf_free(&buf);

    if(sendreliable && !state->lastsentreliable)
        state->lastsentreliable = state->outseq;
}

bool netchan_queue(netchan_t* state, const netbuf_t* msg)
{
    if(state->nmsg && MAX_PACKET - state->msgsizes[state->nmsg-1] <= msg->len)
    {
        memcpy(&state->msg[state->nmsg-1][state->msgsizes[state->nmsg-1]], msg->data, msg->len);
        state->msgsizes[state->nmsg-1] += msg->len;
    }
    else if(state->nmsg >= MAX_MSGQUE)
    {
        state->msgoverflow = true;
        return false;
    }
    else
    {
        state->msgsizes[state->nmsg] = msg->len;
        memcpy(&state->msg[state->nmsg], msg->data, msg->len);
        state->nmsg++;
    }

    return true;
}
