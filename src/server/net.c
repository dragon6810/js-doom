#include "net.h"
#include <rtc/rtc.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

// ---- Receive queue ----
// libdatachannel fires message callbacks on its own internal threads, so the
// queue needs a mutex to stay safe when the game loop drains it on the main thread.

#define NET_QUEUE_LEN 64

typedef struct {
    uint8_t data[NET_MAX_PACKET_SIZE];
    int     size;
} net_packet_t;

static net_packet_t    recv_queue[NET_QUEUE_LEN];
static int             queue_head  = 0;
static int             queue_tail  = 0;
static int             queue_count = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

static int pc_id       = 0;
static int ws_id       = 0;
static int dc_id       = 0;
static int is_connected = 0;

// ---- Internal helpers ----

static void queue_push(const void *data, int size)
{
    if (size <= 0 || size > NET_MAX_PACKET_SIZE) return;

    pthread_mutex_lock(&queue_mutex);
    if (queue_count < NET_QUEUE_LEN) {
        net_packet_t *slot = &recv_queue[queue_tail];
        memcpy(slot->data, data, size);
        slot->size  = size;
        queue_tail  = (queue_tail + 1) % NET_QUEUE_LEN;
        queue_count++;
    } else {
        printf("[net] recv queue full, dropping packet\n");
    }
    pthread_mutex_unlock(&queue_mutex);
}

// ---- libdatachannel callbacks ----

static void local_description_cb(int pc, const char *sdp, const char *type, void *ptr)
{
    cJSON *json    = cJSON_CreateObject();
    cJSON *sdp_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(json,    "type", type);
    cJSON_AddStringToObject(sdp_obj, "type", type);
    cJSON_AddStringToObject(sdp_obj, "sdp",  sdp);
    cJSON_AddItemToObject(json, "sdp", sdp_obj);

    char *json_str = cJSON_PrintUnformatted(json);
    rtcSendMessage(ws_id, json_str, -1);
    free(json_str);
    cJSON_Delete(json);
}

static void local_candidate_cb(int pc, const char *cand, const char *mid, void *ptr)
{
    cJSON *json     = cJSON_CreateObject();
    cJSON *cand_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(json,     "type",     "candidate");
    cJSON_AddStringToObject(cand_obj, "candidate", cand);
    cJSON_AddStringToObject(cand_obj, "sdpMid",    mid);
    cJSON_AddItemToObject(json, "candidate", cand_obj);

    char *json_str = cJSON_PrintUnformatted(json);
    rtcSendMessage(ws_id, json_str, -1);
    free(json_str);
    cJSON_Delete(json);
}

static void data_channel_msg_cb(int dc, const char *msg, int size, void *ptr)
{
    // size >= 0 means binary; size < 0 means text (length = -size - 1)
    if (size >= 0)
        queue_push(msg, size);
}

static void data_channel_cb(int pc, int dc, void *ptr)
{
    printf("[net] peer connected\n");
    dc_id        = dc;
    is_connected = 1;
    rtcSetMessageCallback(dc, data_channel_msg_cb);
}

static void ws_message_cb(int ws, const char *message, int size, void *ptr)
{
    cJSON *json = cJSON_Parse(message);
    if (!json) return;

    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsString(type)) { cJSON_Delete(json); return; }

    if (strcmp(type->valuestring, "offer") == 0) {
        cJSON *sdp_obj  = cJSON_GetObjectItemCaseSensitive(json, "sdp");
        cJSON *sdp_str  = cJSON_GetObjectItemCaseSensitive(sdp_obj, "sdp");
        cJSON *sdp_type = cJSON_GetObjectItemCaseSensitive(sdp_obj, "type");
        if (cJSON_IsString(sdp_str) && cJSON_IsString(sdp_type))
            rtcSetRemoteDescription(pc_id, sdp_str->valuestring, sdp_type->valuestring);

    } else if (strcmp(type->valuestring, "candidate") == 0) {
        cJSON *cand_obj = cJSON_GetObjectItemCaseSensitive(json, "candidate");
        cJSON *cand_str = cJSON_GetObjectItemCaseSensitive(cand_obj, "candidate");
        cJSON *mid_str  = cJSON_GetObjectItemCaseSensitive(cand_obj, "sdpMid");
        if (cJSON_IsString(cand_str) && cJSON_IsString(mid_str))
            rtcAddRemoteCandidate(pc_id, cand_str->valuestring, mid_str->valuestring);
    }

    cJSON_Delete(json);
}

// ---- Public API ----

void net_init(void)
{
    rtcInitLogger(RTC_LOG_WARNING, NULL);

    rtcConfiguration config = {0};
    const char *stun = "stun:stun.l.google.com:19302";
    config.iceServers      = &stun;
    config.iceServersCount = 1;

    pc_id = rtcCreatePeerConnection(&config);
    rtcSetLocalDescriptionCallback(pc_id, local_description_cb);
    rtcSetLocalCandidateCallback(pc_id,   local_candidate_cb);
    rtcSetDataChannelCallback(pc_id,      data_channel_cb);

    ws_id = rtcCreateWebSocket("ws://localhost:8080");
    rtcSetMessageCallback(ws_id, ws_message_cb);

    printf("[net] initialized, waiting for peer...\n");
}

int net_send(const void *data, int size)
{
    if (!is_connected || dc_id == 0) return -1;
    return rtcSendMessage(dc_id, (const char *)data, size);
}

int net_recv_pending(void)
{
    pthread_mutex_lock(&queue_mutex);
    int count = queue_count;
    pthread_mutex_unlock(&queue_mutex);
    return count;
}

int net_recv(void *buf, int buf_size)
{
    pthread_mutex_lock(&queue_mutex);
    if (queue_count == 0) {
        pthread_mutex_unlock(&queue_mutex);
        return 0;
    }
    net_packet_t *slot = &recv_queue[queue_head];
    int copy = slot->size < buf_size ? slot->size : buf_size;
    memcpy(buf, slot->data, copy);
    queue_head  = (queue_head + 1) % NET_QUEUE_LEN;
    queue_count--;
    pthread_mutex_unlock(&queue_mutex);
    return copy;
}

int net_connected(void)
{
    return is_connected;
}
