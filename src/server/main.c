#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "net.h"

int main()
{
    net_init();

    uint8_t buf[NET_MAX_PACKET_SIZE];
    int pkt_size;

    while (1) {
        // Drain every packet that arrived since last tick
        while ((pkt_size = net_recv(buf, sizeof(buf))) > 0) {
            printf("[game] received %d byte packet\n", pkt_size);
            printf("number: %d\n", ntohl(*((int*)buf)));
            // TODO: handle game packet (e.g. parse player command)
        }

        // TODO: run game tick, then send state back:
        //   if (net_connected())
        //       net_send(&gamestate, sizeof(gamestate));

        usleep(50000); // ~20 ticks/sec
    }

    return 0;
}
