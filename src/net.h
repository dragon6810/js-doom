#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdint.h>

#define NET_MAX_PACKET_SIZE 1400

#if __BIG_ENDIAN__
    #ifndef htonll
    #define htonll(x)   (x)
    #endif
    #ifndef ntohll
    #define ntohll(x)   (x)
    #endif
#else
    #ifndef htonll
    #define htonll(x)   ((((uint64_t)htonl(x&0xFFFFFFFF)) << 32) + htonl(x >> 32))
    #endif
    #ifndef ntohll
    #define ntohll(x)   ((((uint64_t)ntohl(x&0xFFFFFFFF)) << 32) + ntohl(x >> 32))
    #endif
#endif

typedef struct
{
    uint8_t* data;
    int len, cap;
} netbuf_t;

extern bool netpacketfull;

void netbuf_init(netbuf_t* buf);
void netbuf_writeu8(netbuf_t* buf, uint8_t val);
void netbuf_writei8(netbuf_t* buf, int8_t val);
void netbuf_writeu16(netbuf_t* buf, uint16_t val);
void netbuf_writei16(netbuf_t* buf, int16_t val);
void netbuf_writeu32(netbuf_t* buf, uint32_t val);
void netbuf_writei32(netbuf_t* buf, int32_t val);
void netbuf_writeu64(netbuf_t* buf, uint64_t val);
void netbuf_writei64(netbuf_t* buf, int64_t val);
void netbuf_writedata(netbuf_t* buf, void* data, int len);
void netbuf_free(netbuf_t* buf);

uint8_t net_readu8(void* data, void* pos, int datalen);
int8_t net_readi8(void* data, void* pos, int datalen);
uint16_t net_readu16(void* data, void* pos, int datalen);
int16_t net_readi16(void* data, void* pos, int datalen);
uint32_t net_readu32(void* data, void* pos, int datalen);
int32_t net_readi32(void* data, void* pos, int datalen);
uint64_t net_readu64(void* data, void* pos, int datalen);
int64_t net_readi64(void* data, void* pos, int datalen);

// Initialize the networking layer. Call once before the game loop.
// Server: connects to the signaling server and waits for a peer.
// Client: sets up the receive queue (call connectToGame() from JS to actually connect).
void net_init(void);

// Returns the number of bytes sent, or -1 if not connected.
int net_send(int dc, const void *data, int size);

// Returns the number of packets sitting in the receive queue.
int net_recv_pending(void);

// Copies the next queued packet into buf (up to buf_size bytes).
// Sets *dc_out to the data channel id the packet arrived on (if dc_out is non-NULL).
// Returns the packet size, or 0 if the queue is empty.
int net_recv(void *buf, int buf_size, int *dc_out);

// Returns 1 if the WebRTC data channel is open and ready, 0 otherwise.
int net_connected(void);

// Returns the data channel id for the server (client only).
int net_server_dc(void);

#endif // NET_H
