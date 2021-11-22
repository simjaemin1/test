#include <linux/rotation.h>
#include <uapi/asm_generic/errno-base.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/slab.h> // kmalloc and kfree
#include <linux/list.h> // kernel list API
#include <mutex.h>

#define INCREMENT 0
#define DECREMENT 1

static int rotation = 0;
static int write_waiting_cnt[360] = {0, };

static LIST_HEAD(read_waiting);
static LIST_HEAD(write_waiting);
static LIST_HEAD(read_acquired);
static LIST_HEAD(write_acquired);

static DEFINE_MUTEX(rotlock_mutex);

void modify_waiting_cnt(int degree, int range, int type)
{
    int pos=degree-range;
    if(pos<0)
        pos+=360;
    for(int i=0; i<=range*2; i++)
    {
        if(pos>=360)
            pos-=360;
        write_waiting_cnt[pos++]+=1-2*type;
    }
}

void exit_rotlock(task_struct *p) // Inject this function into do_exit() in kernel/exit.c
{
    pid_t pid = p->pid;
    rotlock_t *curr;
    rotlock_t *next;

    mutex_lock(&rotlock_mutex);

    // Traverse read_waiting and remove nodes
    list_for_each_entry_safe(curr, next, &read_waiting, node) {
        if(curr->pid == pid) {
            list_del(&curr->node);
        }
    }
    // Traverse write_waiting and remove nodes
    list_for_each_entry_safe(curr, next, &read_waiting, node) {
        if(curr->pid == pid) {
            list_del(&curr->node);
            modify_waiting_cnt(curr->degree, curr->range, DECREMENT);
        }
    }
    // Traverse read_acquired
    if(list_empty(&write_acquired)) {
        // list_empty(&write_acquired) means there are only reading rotlocks.
        list_for_each_entry_safe(curr, next, &read_acquied, node) {
            if(curr->pid == pid) {
                list_del(&curr->node);
            }
        }

        if(list_empty(&read_acquired)) {
            get_lock();
        }
    }

    // Traverse write_acquired
    else if(list_empty(&read_acquired)) {
        // list_empty(&read_acquired) means there is only one writing rotlock.
        curr = list_first_entry(&write_acquired, rotlock_t, node);
        if(curr->pid == pid) {
            list_del(&curr->node);
            get_lock();
        }
    }
    
    mutex_unlock(&rotlock_mutex);
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

int find_node_and_del(int degree, int range, struct list_head* head)
{
    // Don't need to get rotlock_mutex (Already locked)
    // How about identical multiple mutex? (multiple same degree, same range, same pid rotlock_t)
    rotlock_t* curr;
    rotlock_t* next;
    int cnt = 0;

    list_for_each_entry_safe(curr, next, head, node) {
        if(curr->pid == current->pid &&
                curr->degree == degree && curr->range == range) {
            list_del(&curr->node);
            cnt++;
            kfree(curr);
        }
    }
    
    return cnt; // Searching failed
}

SYSCALL_DEFINE1(set_rotation, int, degree)
{
    int cnt;
    rotlock_t *rotlock;

    if(degree < 0 || degree >= 360) {
        printk(KERN_ERR "Degree argument should be between 0 and 360\n");
        return -1;
    }

    cnt = 0;
    rotation = degree;

    if(list_empty(&write_acquired)) {
        // Reading


    }
    else if(list_empty(&write_acquired)) {
        // Writing
    }

    list_for_each_entry_safe() {
        check_range();
    }

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
    int cnt;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -EINVAL;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -EINVAL;
    }

    mutex_lock(&rotlock_mutex);

    cnt = find_node_and_del(degree, range, &read_acquired);
    
    if(!cnt) {
        mutex_unlock(&rotlock_mutex);
        printk(KERN_ERR "Can't find acquired read lock with degree %d and range %d\n", degree, range);
        return -1; // fail
    }

    if(list_empty(&read_acquired)) {
        get_lock();
    }

    mutex_unlock(&rotlock_mutex);
    return 0; // success
}

SYSCALL_DEFINE2(rotunlock_write, int, degree, int, range)
{
    int cnt;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -EINVAL;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -EINVAL;
    }

    mutex_lock(&rotlock_mutex);

    cnt = find_node_and_del(degree, range, &write_acquired);

    if(!cnt) {
        mutex_unlock(&rotlock_mutex);
        printk(KERN_ERR "Can't find acquired write lock with degree %d and range %d\n", degree, range);
        return -1; // fail
    }

    get_lock();

    mutex_unlock(&rotlock_mutex);
    return 0; // success
}
