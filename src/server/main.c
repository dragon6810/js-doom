#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#include "client.h"
#include "net.h"

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
    printf("tic\n");
}

int main()
{
    uint64_t nexttic, now;

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
