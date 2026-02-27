#include "net.h"

#include <arpa/inet.h>
#include <stdlib.h>

#define NETBUF_MINSIZE 32

bool netpacketfull;

static void netbuf_resize(netbuf_t* buf)
{
    buf->cap *= 2;
    buf->data = realloc(buf->data, buf->len);
}

void netbuf_init(netbuf_t* buf)
{
    buf->data = malloc(NETBUF_MINSIZE);
    buf->len = 0;
    buf->cap = NETBUF_MINSIZE;
}

void netbuf_writeu8(netbuf_t* buf, uint8_t val)
{
    while(buf->len + sizeof(uint8_t) >= buf->cap)
        netbuf_resize(buf);
    
    *(uint8_t*) (buf->data + buf->len) = val;
}

void netbuf_writei8(netbuf_t* buf, int8_t val)
{
    while(buf->len + sizeof(int8_t) >= buf->cap)
        netbuf_resize(buf);
    
    *(int8_t*) (buf->data + buf->len) = val;
}

void netbuf_writeu16(netbuf_t* buf, uint16_t val)
{
    while(buf->len + sizeof(uint16_t) >= buf->cap)
        netbuf_resize(buf);
    
    *(uint16_t*) (buf->data + buf->len) = htons(val);
}

void netbuf_writei16(netbuf_t* buf, int16_t val)
{
    while(buf->len + sizeof(int16_t) >= buf->cap)
        netbuf_resize(buf);
    
    *(int16_t*) (buf->data + buf->len) = htons(val);
}

void netbuf_writeu32(netbuf_t* buf, uint32_t val)
{
    while(buf->len + sizeof(uint32_t) >= buf->cap)
        netbuf_resize(buf);
    
    *(uint32_t*) (buf->data + buf->len) = htonl(val);
}

void netbuf_writei32(netbuf_t* buf, int32_t val)
{
    while(buf->len + sizeof(int32_t) >= buf->cap)
        netbuf_resize(buf);
    
    *(int32_t*) (buf->data + buf->len) = htonl(val);
}

void netbuf_writeu64(netbuf_t* buf, uint64_t val)
{
    while(buf->len + sizeof(uint64_t) >= buf->cap)
        netbuf_resize(buf);
    
    *(uint64_t*) (buf->data + buf->len) = htonll(val);
}

void netbuf_writei64(netbuf_t* buf, int64_t val)
{
    while(buf->len + sizeof(int64_t) >= buf->cap)
        netbuf_resize(buf);
    
    *(int64_t*) (buf->data + buf->len) = htonll(val);
}

void netbuf_free(netbuf_t* buf)
{
    if(buf->data)
        free(buf->data);

    buf->data = NULL;
    buf->len = buf->cap = 0;
}


uint8_t net_readu8(void* data, void* pos, int datalen)
{
    netpacketfull = false;

    if(pos - data + sizeof(uint8_t) >= datalen)
    {
        netpacketfull = true;
        return 0;
    }

    return *(uint8_t*) pos;
}

int8_t net_readi8(void* data, void* pos, int datalen)
{
    netpacketfull = false;

    if(pos - data + sizeof(int8_t) >= datalen)
    {
        netpacketfull = true;
        return 0;
    }

    return *(int8_t*) pos;
}

uint16_t net_readu16(void* data, void* pos, int datalen)
{
    netpacketfull = false;

    if(pos - data + sizeof(uint16_t) >= datalen)
    {
        netpacketfull = true;
        return 0;
    }

    return ntohs(*(uint16_t*) pos);
}

int16_t net_readi16(void* data, void* pos, int datalen)
{
    netpacketfull = false;

    if(pos - data + sizeof(int16_t) >= datalen)
    {
        netpacketfull = true;
        return 0;
    }

    return ntohs(*(int16_t*) pos);
}

uint32_t net_readu32(void* data, void* pos, int datalen)
{
    netpacketfull = false;

    if(pos - data + sizeof(uint32_t) >= datalen)
    {
        netpacketfull = true;
        return 0;
    }

    return ntohl(*(uint32_t*) pos);
}

int32_t net_readi32(void* data, void* pos, int datalen)
{
    netpacketfull = false;

    if(pos - data + sizeof(int32_t) >= datalen)
    {
        netpacketfull = true;
        return 0;
    }

    return ntohl(*(int32_t*) pos);
}

uint64_t net_readu64(void* data, void* pos, int datalen)
{
    netpacketfull = false;

    if(pos - data + sizeof(uint64_t) >= datalen)
    {
        netpacketfull = true;
        return 0;
    }

    return ntohll(*(uint64_t*) pos);
}

int64_t net_readi64(void* data, void* pos, int datalen)
{
    netpacketfull = false;

    if(pos - data + sizeof(int64_t) >= datalen)
    {
        netpacketfull = true;
        return 0;
    }

    return ntohll(*(int64_t*) pos);
}
