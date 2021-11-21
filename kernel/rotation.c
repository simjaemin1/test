#include <linux/rotation.h>
#include <uapi/asm_generic/errno-base.h>
#include <linux/syscalls.h>
#include <linux/slab.h> // kmalloc and kfree
#include <linux/list.h> // kernel list API
#include <mutex.h>

static struct rotlock_t read_waiting;

rotlock_t* init_rotlock(int degree, int range, int rw_type) 
{
    rotlock_t* rotlock;
    rotlock = (rotlock_t *)kmalloc(sizeof(rotlock_t));
    rotlock.degree = degree;
    rotlock.range = range;
    rotlock.rw_type = rw_type;
    INIT_LIST_HEAD(&rotlock.node);

    return rotlock;
}

SYSCALL_DEFINE1(set_rotation, int, degree)
{
}

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
{
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
}
