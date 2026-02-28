#include "net.h"
#include "client.h"
#include <rtc/rtc.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

// ---- Receive queue ----

#define NET_QUEUE_LEN 64
#define MAX_PEERS 8

typedef struct
{
    uint8_t data[NET_MAX_PACKET_SIZE];
    int size;
    int dc_id;
} net_packet_t;

typedef struct
{
    int clientid;
    int pc;
} peer_t;

static net_packet_t    recv_queue[NET_QUEUE_LEN];
static int             queue_head  = 0;
static int             queue_tail  = 0;
static int             queue_count = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

static peer_t peers[MAX_PEERS];
static int    npeers = 0;
static int    ws_id  = 0;

// ---- Internal helpers ----

static void queue_push(int dc, const void *data, int size)
{
    if (size <= 0 || size > NET_MAX_PACKET_SIZE) return;

    pthread_mutex_lock(&queue_mutex);
    if (queue_count < NET_QUEUE_LEN) {
        net_packet_t *slot = &recv_queue[queue_tail];
        memcpy(slot->data, data, size);
        slot->size  = size;
        slot->dc_id = dc;
        queue_tail  = (queue_tail + 1) % NET_QUEUE_LEN;
        queue_count++;
    } else {
        printf("[net] recv queue full, dropping packet\n");
    }
    pthread_mutex_unlock(&queue_mutex);
}

static peer_t* findpeer(int clientid)
{
    int i;
    for(i=0; i<npeers; i++)
        if(peers[i].clientid == clientid)
            return &peers[i];
    return NULL;
}

static peer_t* allocpeer(int clientid)
{
    if(npeers >= MAX_PEERS)
        return NULL;
    peers[npeers].clientid = clientid;
    peers[npeers].pc = 0;
    return &peers[npeers++];
}

// ---- libdatachannel callbacks ----

static void local_description_cb(int pc, const char *sdp, const char *type, void *ptr)
{
    peer_t *peer = (peer_t*) ptr;
    cJSON *json    = cJSON_CreateObject();
    cJSON *sdp_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(json,    "clientId", peer->clientid);
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
    peer_t *peer = (peer_t*) ptr;
    cJSON *json     = cJSON_CreateObject();
    cJSON *cand_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(json,     "clientId",  peer->clientid);
    cJSON_AddStringToObject(json,     "type",      "candidate");
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
    if (size >= 0)
        queue_push(dc, msg, size);
}

static void data_channel_cb(int pc, int dc, void *ptr)
{
    printf("[net] peer connected on dc %d\n", dc);
    rtcSetMessageCallback(dc, data_channel_msg_cb);
}

static void ws_open_cb(int ws, void *ptr)
{
    rtcSendMessage(ws_id, "{\"type\":\"register-server\"}", -1);
    printf("[net] registered with signaling server\n");
}

static void ws_message_cb(int ws, const char *message, int size, void *ptr)
{
    rtcConfiguration config;
    const char *stun;
    int clientid;
    peer_t *peer;
    cJSON *json, *type_item, *clientid_item, *sdp_obj, *cand_obj;
    cJSON *sdp_str, *sdp_type, *cand_str, *mid_str;

    json = cJSON_Parse(message);
    if (!json) return;

    type_item     = cJSON_GetObjectItemCaseSensitive(json, "type");
    clientid_item = cJSON_GetObjectItemCaseSensitive(json, "clientId");

    if (!cJSON_IsString(type_item) || !cJSON_IsNumber(clientid_item))
    {
        cJSON_Delete(json);
        return;
    }

    clientid = (int) clientid_item->valuedouble;

    if (strcmp(type_item->valuestring, "offer") == 0)
    {
        peer = findpeer(clientid);
        if (!peer)
            peer = allocpeer(clientid);
        if (!peer)
        {
            printf("[net] too many peers\n");
            cJSON_Delete(json);
            return;
        }

        if (!peer->pc)
        {
            memset(&config, 0, sizeof(config));
            stun = "stun:stun.l.google.com:19302";
            config.iceServers      = &stun;
            config.iceServersCount = 1;

            peer->pc = rtcCreatePeerConnection(&config);
            rtcSetUserPointer(peer->pc, peer);
            rtcSetLocalDescriptionCallback(peer->pc, local_description_cb);
            rtcSetLocalCandidateCallback(peer->pc,   local_candidate_cb);
            rtcSetDataChannelCallback(peer->pc,      data_channel_cb);
        }

        sdp_obj = cJSON_GetObjectItemCaseSensitive(json, "sdp");
        if (sdp_obj)
        {
            sdp_str  = cJSON_GetObjectItemCaseSensitive(sdp_obj, "sdp");
            sdp_type = cJSON_GetObjectItemCaseSensitive(sdp_obj, "type");
            if (cJSON_IsString(sdp_str) && cJSON_IsString(sdp_type))
                rtcSetRemoteDescription(peer->pc, sdp_str->valuestring, sdp_type->valuestring);
        }
    }
    else if (strcmp(type_item->valuestring, "candidate") == 0)
    {
        peer = findpeer(clientid);
        if (!peer)
        {
            cJSON_Delete(json);
            return;
        }

        cand_obj = cJSON_GetObjectItemCaseSensitive(json, "candidate");
        if (cand_obj)
        {
            cand_str = cJSON_GetObjectItemCaseSensitive(cand_obj, "candidate");
            mid_str  = cJSON_GetObjectItemCaseSensitive(cand_obj, "sdpMid");
            if (cJSON_IsString(cand_str) && cJSON_IsString(mid_str))
                rtcAddRemoteCandidate(peer->pc, cand_str->valuestring, mid_str->valuestring);
        }
    }

    cJSON_Delete(json);
}

// ---- Public API ----

void net_init(void)
{
    rtcInitLogger(RTC_LOG_WARNING, NULL);

    ws_id = rtcCreateWebSocket("ws://localhost:8080");
    rtcSetOpenCallback(ws_id,    ws_open_cb);
    rtcSetMessageCallback(ws_id, ws_message_cb);

    printf("[net] initialized, waiting for peers...\n");
}

int net_send(int dc, const void *data, int size)
{
    if(!dc)
        return -1;
    return rtcSendMessage(dc, (const char *)data, size);
}

int net_recv_pending(void)
{
    pthread_mutex_lock(&queue_mutex);
    int count = queue_count;
    pthread_mutex_unlock(&queue_mutex);
    return count;
}

int net_recv(void *buf, int buf_size, int *dc_out)
{
    pthread_mutex_lock(&queue_mutex);
    if (queue_count == 0) {
        pthread_mutex_unlock(&queue_mutex);
        return 0;
    }
    net_packet_t *slot = &recv_queue[queue_head];
    int copy = slot->size < buf_size ? slot->size : buf_size;
    memcpy(buf, slot->data, copy);
    if (dc_out) *dc_out = slot->dc_id;
    queue_head  = (queue_head + 1) % NET_QUEUE_LEN;
    queue_count--;
    pthread_mutex_unlock(&queue_mutex);
    return copy;
}

int net_connected(void)
{
    return true;
}
