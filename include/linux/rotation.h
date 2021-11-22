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


int get_lock();
int check_range(int rotation, int degree, int range);
void modify_waiting_cnt(int degree, int range, int type);
rotlock_t* init_rotlock(int degree, int range, int rw_type);
rotlock_t* find_node_and_del(int degree, int range, struct list_head* head);
void exit_rotlock(task_struct *p);
#endif
