#ifndef _MUS_H
#define _MUS_H

#include <stdint.h>

void    mus_init(void);
void    mus_start(int episode, int map);
void    mus_mixbuf(int32_t *lbuf, int32_t *rbuf, int n);

#endif
