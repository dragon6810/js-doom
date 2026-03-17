#ifndef _MUS_H
#define _MUS_H

#include <stdint.h>

void    mus_init(void);
void    mus_start(int episode, int map);
void    mus_samplemix(int32_t *l, int32_t *r);

#endif
