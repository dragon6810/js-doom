#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "client.h"
#include "net.h"
#include "wad.h"
#include "level.h"

#define TICRATE         35
#define TICMICROSECONDS (1000000 / TICRATE)

static uint64_t nowmicro(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
}

static void tic(void)
{
    recvfromclients();

    sendtoclients();
}

int ep = -1, map = -1;

static void parseargs(int argc, char** argv)
{
    int i;

    for(i=0; i<argc; i++)
    {
        if((!strcasecmp(argv[i], "-i") || !strcasecmp(argv[i], "-iwad")) && i < argc-1)
        {
            wad_load(argv[i + 1]);
            i++;
        }
        else if((!strcasecmp(argv[i], "-w") || !strcasecmp(argv[i], "-warp")) && i < argc-2)
        {
            ep = argv[i+1][0] - '0';
            map = argv[i+2][0] - '0';
            i += 2;
            if(ep < 1 || ep > 4 || map < 1 || map > 9)
                ep = map = -1;
        }
    }
}

int main(int argc, char** argv)
{
    uint64_t nexttic, now;

    parseargs(argc - 1, argv + 1);

    if(ep == -1 || map == -1)
    {
        printf("valid map not specified (use -warp)\n");
        return 1;
    }

    level_load(ep, map);

    net_init();

    nexttic = nowmicro();

    while (1)
    {
        now = nowmicro();

        while (now >= nexttic)
        {
            tic();
            nexttic += TICMICROSECONDS;
        }

        now = nowmicro();
        if (nexttic > now)
            usleep(nexttic - now);
    }

    return 0;
}
