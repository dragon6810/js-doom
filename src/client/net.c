#include "net.h"
#include <emscripten.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// ---- Receive queue ----
// Emscripten (without -pthread) is single-threaded: both the game loop and the
// WebRTC onmessage callback run on the browser's main thread, so no mutex is needed.

#define NET_QUEUE_LEN 64

typedef struct {
    uint8_t data[NET_MAX_PACKET_SIZE];
    int     size;
} net_packet_t;

static net_packet_t recv_queue[NET_QUEUE_LEN];
static int          queue_head  = 0;
static int          queue_tail  = 0;
static int          queue_count = 0;

// Integer handle for the server's data channel (set by net_set_dc, called from JS).
static int server_dc = 0;

// ---- Called from JavaScript when a packet arrives on the data channel ----
// pre.js calls: Module._net_push_packet(ptr, size)

EMSCRIPTEN_KEEPALIVE
void net_push_packet(const void *data, int size)
{
    if (size <= 0 || size > NET_MAX_PACKET_SIZE) return;
    if (queue_count >= NET_QUEUE_LEN) {
        printf("[net] recv queue full, dropping packet\n");
        return;
    }
    net_packet_t *slot = &recv_queue[queue_tail];
    memcpy(slot->data, data, size);
    slot->size  = size;
    queue_tail  = (queue_tail + 1) % NET_QUEUE_LEN;
    queue_count++;
}

// Called from JS when the WebRTC data channel to the server opens.
// pre.js: Module._netDataChannels[id] = dc; Module._net_set_dc(id);
EMSCRIPTEN_KEEPALIVE
void net_set_dc(int id)
{
    server_dc = id;
    printf("[net] server data channel registered as dc %d\n", id);
}

// ---- JS bridges ----

EM_JS(int, js_net_send, (int dc, const void *data, int size), {
    var ch = (Module._netDataChannels || {})[dc];
    if (!ch || ch.readyState !== 'open') return -1;
    // slice creates an independent ArrayBuffer copy so the C stack frame can return safely
    ch.send(HEAPU8.buffer.slice(data, data + size));
    return size;
});

EM_JS(int, js_net_connected, (int dc), {
    var ch = (Module._netDataChannels || {})[dc];
    return (ch && ch.readyState === 'open') ? 1 : 0;
});

// ---- Public API ----

void net_init(void)
{
    printf("[net] client net ready (call connectToGame() from JS to connect)\n");
}

int net_send(int dc, const void *data, int size)
{
    return js_net_send(dc, data, size);
}

int net_recv_pending(void)
{
    return queue_count;
}

int net_recv(void *buf, int buf_size, int *dc_out)
{
    if (queue_count == 0) return 0;
    net_packet_t *slot = &recv_queue[queue_head];
    int copy = slot->size < buf_size ? slot->size : buf_size;
    memcpy(buf, slot->data, copy);
    if (dc_out) *dc_out = server_dc;
    queue_head  = (queue_head + 1) % NET_QUEUE_LEN;
    queue_count--;
    return copy;
}

int net_connected(void)
{
    return js_net_connected(server_dc);
}

int net_server_dc(void)
{
    return server_dc;
}
