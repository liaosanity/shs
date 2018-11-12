#ifndef SHS_CHANNEL_H
#define SHS_CHANNEL_H

#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

typedef struct 
{
    uint32_t command;
    pid_t    pid;
    int      slot;
    int      fd;
} shs_channel_t;

int  shs_write_channel(int s, shs_channel_t *ch, size_t size);
int  shs_read_channel(int s, shs_channel_t *ch, size_t size);
void shs_close_channel(int *fd);

#endif
