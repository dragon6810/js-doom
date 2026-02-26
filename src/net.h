#ifndef NET_H
#define NET_H

// Maximum bytes in a single packet
#define NET_MAX_PACKET_SIZE 1400

// Initialize the networking layer. Call once before the game loop.
// Server: connects to the signaling server and waits for a peer.
// Client: sets up the receive queue (call connectToGame() from JS to actually connect).
void net_init(void);

// Send a packet to the connected peer.
// Returns the number of bytes sent, or -1 if not connected.
int net_send(const void *data, int size);

// Returns the number of packets sitting in the receive queue.
int net_recv_pending(void);

// Copies the next queued packet into buf (up to buf_size bytes).
// Returns the packet size, or 0 if the queue is empty.
int net_recv(void *buf, int buf_size);

// Returns 1 if the WebRTC data channel is open and ready, 0 otherwise.
int net_connected(void);

#endif // NET_H
