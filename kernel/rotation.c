#include <linux/rotation.h>
#include <uapi/asm_generic/errno-base.h>
#include <linux/syscalls.h>
#include <linux/slab.h> // kmalloc and kfree
#include <linux/list.h> // kernel list API
#include <mutex.h>

static int rotation = 0;

static LIST_HEAD(read_waiting);
static LIST_HEAD(write_waiting);
static LIST_HEAD(read_active);
static LIST_HEAD(write_active);

static DEFINE_MUTEX(rotlock_mutex);

rotlock_t* init_rotlock(int degree, int range, int rw_type) 
{
    rotlock_t* rotlock;
    rotlock = (rotlock_t *)kmalloc(sizeof(rotlock_t));
    if(!rotlock)
        return NULL;
    rotlock.pid = getpid();
    rotlock.degree = degree;
    rotlock.range = range;
    rotlock.rw_type = rw_type;
    INIT_LIST_HEAD(&rotlock.node);

    return rotlock;
}

SYSCALL_DEFINE1(set_rotation, int, degree)
{
    if(degree < 0 || degree >= 360) {
        printk(KERN_ERR "Degree argument should be between 0 and 360\n");
        return -EINVAL;
    }

    rotation = degree;
}

// We should check the condition degree - range <= LOCK RANGE <= degree + range

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
{
    rotlock_t *rotlock;
    int cnt;

    if(degree < 0 || degree >= 360) {
        printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -EINVAL;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -EINVAL;
    }

    rotlock = init_rotlock(degree, range, READ);

    if(!rotlock) {
        printk(KERN_ERR "kmalloc failed\n");
        return -ENOMEM;
    }

    return -1;
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
    rotlock_t *rotlock;
    int cnt;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -EINVAL;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -EINVAL;
    }

    rotlock = init_rotlock(degree, range, WRITE);
    
    if(!rotlock) {
        printk(KERN_ERR "kmalloc failed\n");
        return -ENOMEM;
    }
    return -1;
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
    rotlock_t *rotlock;
    int cnt;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -EINVAL;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -EINVAL;
    }

    //rotlock = init_rotlock(degree, range, READ);

    return -1;
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
    rotlock_t *rotlock;
    int cnt;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -EINVAL;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -EINVAL;
    }

    //rotlock = init_rotlock(degree, range, WRITE);

    return -1;
}
