#include <rtc/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <cJSON.h> // Used for parsing the signaling messages

int pc_id = 0; // PeerConnection ID
int ws_id = 0; // WebSocket ID

// Catches the automatically generated 'Answer' and sends it to the browser
void local_description_cb(int pc, const char *sdp, const char *type, void *ptr) {
    printf("Generated local %s, sending to signaling server...\n", type);
    
    // Create JSON: { "type": "answer", "sdp": { "type": "answer", "sdp": "v=0..." } }
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "type", type);
    
    cJSON *sdp_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(sdp_obj, "type", type);
    cJSON_AddStringToObject(sdp_obj, "sdp", sdp);
    cJSON_AddItemToObject(json, "sdp", sdp_obj);
    
    char *json_str = cJSON_PrintUnformatted(json);
    
    // Send it over the WebSocket! (-1 tells libdatachannel to send as a text string)
    rtcSendMessage(ws_id, json_str, -1); 
    
    free(json_str);
    cJSON_Delete(json);
}

// Listens to the Signaling Server
void ws_message_cb(int ws, const char *message, int size, void *ptr) {
    cJSON *json = cJSON_Parse(message);
    if (!json) return;

    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (cJSON_IsString(type) && (type->valuestring != NULL)) {
        
        if (strcmp(type->valuestring, "offer") == 0) {
            printf("Received offer from browser!\n");
            cJSON *sdp_obj = cJSON_GetObjectItemCaseSensitive(json, "sdp");
            if (sdp_obj) {
                cJSON *sdp_string = cJSON_GetObjectItemCaseSensitive(sdp_obj, "sdp");
                cJSON *sdp_type = cJSON_GetObjectItemCaseSensitive(sdp_obj, "type");
                
                if (cJSON_IsString(sdp_string) && cJSON_IsString(sdp_type)) {
                    // Feed the offer into WebRTC. This will automatically trigger local_description_cb!
                    rtcSetRemoteDescription(pc_id, sdp_string->valuestring, sdp_type->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "candidate") == 0) {
            cJSON *candidate_obj = cJSON_GetObjectItemCaseSensitive(json, "candidate");
            if (candidate_obj) {
                cJSON *cand_str = cJSON_GetObjectItemCaseSensitive(candidate_obj, "candidate");
                cJSON *mid_str = cJSON_GetObjectItemCaseSensitive(candidate_obj, "sdpMid");
                
                if (cJSON_IsString(cand_str) && cJSON_IsString(mid_str)) {
                    // Give the client's network route to WebRTC
                    rtcAddRemoteCandidate(pc_id, cand_str->valuestring, mid_str->valuestring);
                }
            }
        }
    }
    
    cJSON_Delete(json);
}

// 1. Handle incoming binary game data from the client
void data_channel_msg_cb(int dc, const char *msg, int size, void *ptr) {
    // If size >= 0, it's a binary packet. If < 0, it's a text string.
    if (size >= 0) {
        printf("Received %d bytes of binary game data!\n", size);
        // TODO: Pass 'msg' into your Doom game state logic!
    }
}

// 2. Triggered when the client successfully opens the direct WebRTC channel
void data_channel_cb(int pc, int dc, void *ptr) {
    printf("🔥 Client opened the direct Data Channel!\n");
    rtcSetMessageCallback(dc, data_channel_msg_cb);
}

// Triggered when the server finds its own network route (ICE candidate)
void local_candidate_cb(int pc, const char *cand, const char *mid, void *ptr) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "type", "candidate");
    
    cJSON *cand_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(cand_obj, "candidate", cand);
    cJSON_AddStringToObject(cand_obj, "sdpMid", mid);
    cJSON_AddItemToObject(json, "candidate", cand_obj);
    
    char *json_str = cJSON_PrintUnformatted(json);
    rtcSendMessage(ws_id, json_str, -1);
    
    free(json_str);
    cJSON_Delete(json);
}

int main() {
    // Initialize WebRTC
    rtcInitLogger(RTC_LOG_WARNING, NULL);

    // Setup STUN server configuration
    rtcConfiguration config = {0};
    const char *stun_server = "stun:stun.l.google.com:19302";
    config.iceServers = &stun_server;
    config.iceServersCount = 1;

    // Create the PeerConnection
    pc_id = rtcCreatePeerConnection(&config);
    
    // Wire up the WebRTC callbacks (Notice ws_message_cb is removed from here)
    rtcSetLocalDescriptionCallback(pc_id, local_description_cb);
    rtcSetDataChannelCallback(pc_id, data_channel_cb);
    rtcSetLocalCandidateCallback(pc_id, local_candidate_cb);

    // 1. Create the WebSocket for signaling
    ws_id = rtcCreateWebSocket("ws://localhost:8080");
    
    // 2. NOW attach the message callback using ws_id!
    rtcSetMessageCallback(ws_id, ws_message_cb);

    printf("Server running! Waiting for connection...\n");

    // Keep the C server alive
    while (1) {
        sleep(1);
    }

    return 0;
}