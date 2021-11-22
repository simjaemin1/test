#include <linux/rotation.h>
#include <uapi/asm_generic/errno-base.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/slab.h> // kmalloc and kfree
#include <linux/list.h> // kernel list API
#include <mutex.h>

static int rotation = 0;

static LIST_HEAD(read_waiting);
static LIST_HEAD(write_waiting);
static LIST_HEAD(read_acquired);
static LIST_HEAD(write_acquired);

static DEFINE_MUTEX(rotlock_mutex); // need more mutex or not?

void exit_rotlock(task_struct *p) // Inject this function into do_exit() in kernel/exit.c
{
    pid_t pid = p->pid;

}

rotlock_t* init_rotlock(int degree, int range, int rw_type) 
{
    rotlock_t* rotlock;
    rotlock = (rotlock_t *)kmalloc(sizeof(rotlock_t));
    if(!rotlock)
        return NULL;
    rotlock.pid = current->pid;
    rotlock.degree = degree;
    rotlock.range = range;
    rotlock.rw_type = rw_type;
    INIT_LIST_HEAD(&rotlock.node);

    return rotlock;
}

rotlock_t* find_lock(int degree, int range, struct list_head* head)
{
    //struct list_head* pos;
    rotlock_t* curr;
    //rotlock_t* next;
    /*list_for_each(pos, head) { // list_for_each_entry_safe()?
        rotlock = container_of(pos, rotlock_t, node);
        if(rotlock->pid == current->pid && 
                rotlock->degree == degree && rotlock->range == range)
            return rotlock;
    }

    list_for_each_entry_safe(curr, next, head, node) {
        if(curr->pid == current->pid &&
                curr->degree == degree && curr->range == range) {
            list_del(&curr->node); // ??
            return rotlock;
        }
    }*/

    list_for_each_entry(curr, head, node) {
        if(curr->pid == current->pid &&
                curr->degree == degree && curr->range == range) {
            list_del(&curr->node);
            return rotlock;
        }
    }
    
    return NULL; // Searching failed
}

SYSCALL_DEFINE1(set_rotation, int, degree)
{
    int cnt;

    if(degree < 0 || degree >= 360) {
        printk(KERN_ERR "Degree argument should be between 0 and 360\n");
        return -1;
    }

    cnt = 0;
    rotation = degree;

    return cnt;
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

    mutex_lock(&rotlock_mutex);

    

    mutex_unlock(&rotlock_mutex);

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

    mutex_lock(&rotlock_mutex);

    

    mutex_unlock(&rotlock_mutex);

    return -1;
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
    rotlock_t *rotlock;
    int result;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -EINVAL;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -EINVAL;
    }

    mutex_lock(&rotlock_mutex);

    rotlock = find_node(degree, range, &read_acquired);
    
    if(!rotlock) {
        mutex_unlock(&rotlock_mutex);
        printk(KERN_ERR "Can't find acquired read lock with degree %d and range %d\n", degree, range);
        return -1;
    }

    mutex_unlock(&rotlock_mutex);

    kfree(rotlock);
    return -1;
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
    rotlock_t *rotlock;
    int result;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -EINVAL;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -EINVAL;
    }

    mutex_lock(&rotlock_mutex);

    rotlock = find_node(degree, range, &write_acquired);

    if(!rotlock) {
        mutex_unlock(&rotlock_mutex);
        printk(KERN_ERR "Can't find acquired write lock with degree %d and range %d\n", degree, range);
        return -1;
    }

    mutex_unlock(&rotlock_mutex);

    kfree(rotlock);
    return -1;
}
