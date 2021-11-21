#ifndef __LINUX_ROTATION_H__
#define __LINUX_ROTATION_H__

#include <linux/list.h>
#include <linux/types.h>

#define READ    0
#define WRITE   1

struct __rotlock_t__ {
    pid_t pid;
    int degree;
    int range;
    int rw_type; // Process is read or write type.
    struct list_head node;
} rotlock_t

#endif
