#include <linux/rotation.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/slab.h> // kmalloc and kfree
#include <linux/list.h> // kernel list API
#include <linux/mutex.h>
#include <linux/wait.h>

#define INCREMENT 0
#define DECREMENT 1

static int rotation = 0;
static int write_waiting_cnt[360] = {0, };

static LIST_HEAD(read_waiting);
static LIST_HEAD(write_waiting);
static LIST_HEAD(read_acquired);
static LIST_HEAD(write_acquired);

static DEFINE_MUTEX(rotlock_mutex);
static DECLARE_WAIT_QUEUE_HEAD(lock_wait_queue);

// static int cnt = 0;
void wait(rotlock_t * curr)
{
	curr->condition=0;
	DEFINE_WAIT(wait_entry);
	add_wait_queue(&wait_queue, &wait_entry);
	while(!curr>condition) {
		prepare_to_wait(&wait_queue, &wait,	TASK_INTERRUPIBLE);
		schedule();
	}
	finish_wait(&q, &wait);

}

void wakeup(rotlock_t * curr)
{
	curr->condition=1;
	wake_up_task(find_process_by_pid(curr->pid));
}

int get_lock(void) {
    
    rotlock_t *curr;
    int cnt = 0;

    if(!list_empty(&write_acquired)) {
        // Writer is using -> Don't need to try lock -> return
        return 0;
    }

    if(write_waiting_cnt[rotation] != 0 && list_empty(&read_acquired)) {
        // Try to get lock (writer) only if read_acquired is empty (readers are using)
        list_for_each_entry(curr, &write_waiting, node) {
            if(check_range(rotation, curr->degree, curr->range)) {
                cnt++;
                wakeup(curr);
                return cnt;
            }
        }
    }
    else if(write_waiting_cnt[rotation] == 0) {
        // Try to get new read waiting lock
        list_for_each_entry(curr, &read_waiting, node) {
            if(check_range(rotation, curr->degree, curr->range)) {
                cnt++;
                wakeup(curr);
            }
        }
    }

    return cnt;
}

int check_range(int rotation, int degree, int range)
{
    int low = degree-range;
    int high = degree+range;
    if(low<0)
        return (low+360<=rotation&&rotation<360)||(rotation<=high);
    if(high>=360)
        return (low<=rotation)||(rotation<=high-360);

    return (low<=rotation)&&(rotation<=high);
}

void modify_waiting_cnt(int degree, int range, int type)
{   int i;
    int pos;
    pos = degree - range;

    if(pos < 0)
        pos += 360;

    for(i = 0; i <= range*2; i++)
    {
        if (pos >= 360)
            pos -= 360;
        write_waiting_cnt[pos++] += 1 - 2*type;
    }
}

void exit_rotlock(struct task_struct *p) // Inject this function into do_exit() in kernel/exit.c
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
        list_for_each_entry_safe(curr, next, &read_acquired, node) {
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
    //printk(KERN_INFO "exit_rotlock successfully returned\n");
}

rotlock_t* init_rotlock(int degree, int range) 
{
    rotlock_t* rotlock;
    rotlock = (rotlock_t *)kmalloc(sizeof(rotlock_t), GFP_KERNEL);
    if(!rotlock)
        return NULL;
    rotlock->pid = current->pid;
    rotlock->degree = degree;
    rotlock->range = range;
    rotlock->cond = 0;
    INIT_LIST_HEAD(&rotlock->node);

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
    int cnt = 0;

    if(degree < 0 || degree >= 360) {
        printk(KERN_ERR "Degree argument should be between 0 and 360\n");
        return -1;
    }

    mutex_lock(&rotlock_mutex);

    rotation = degree;

    cnt = get_lock();

    mutex_unlock(&rotlock_mutex);

    return cnt;
}

// We should check the condition degree - range <= LOCK RANGE <= degree + range

SYSCALL_DEFINE2(rotlock_read, int, degree, int, range)
{
    rotlock_t *rotlock;

    if(degree < 0 || degree >= 360) {
        printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -1;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -1;
    }

    rotlock = init_rotlock(degree, range);

    if(!rotlock) {
        printk(KERN_ERR "kmalloc failed\n");
        return -1;
    }

    mutex_lock(&rotlock_mutex);


    if(check_range(rotation, degree, range) 
            && write_waiting_cnt[rotation] == 0 && list_empty(&write_acquired)) {
        list_add_tail(&rotlock->node, &read_acquired);
        mutex_unlock(&rotlock_mutex);
        return 0;
    }
    else {
        list_add_tail(&rotlock->node, &read_waiting);
        while(!check_range(rotation, degree, range) 
                || write_waiting_cnt[rotation] != 0 || !list_empty(&write_acquired)) {
            mutex_unlock(&rotlock_mutex);
            wait(rotlock);
            mutex_lock(&rotlock_mutex);
        }
    }

    list_del(&rotlock->node);
    list_add_tail(&rotlock->node, &read_acquired);

    mutex_unlock(&rotlock_mutex);

    return 0;
}

SYSCALL_DEFINE2(rotlock_write, int, degree, int, range)
{
    rotlock_t *rotlock;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -1;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -1;
    }

    rotlock = init_rotlock(degree, range);
    
    if(!rotlock) {
        printk(KERN_ERR "kmalloc failed\n");
        return -1;
    }

    mutex_lock(&rotlock_mutex);    

    if(check_range(rotation, degree, range) 
            && list_empty(&read_acquired) && list_empty(&write_acquired)) {
        list_add_tail(&rotlock->node, &write_acquired);
        mutex_unlock(&rotlock_mutex);
        return 0;
    }
    else {
        list_add_tail(&rotlock->node, &write_waiting);
        modify_waiting_cnt(degree, range, INCREMENT);
        while(!check_range(rotation, degree, range) 
                || !list_empty(&read_acquired) || !list_empty(&write_acquired)) {
            mutex_unlock(&rotlock_mutex);
            wait(rotlock);
            mutex_lock(&rotlock_mutex);
        }
    }
   
    modify_waiting_cnt(degree, range, DECREMENT); 
    list_del(&rotlock->node);
    list_add_tail(&rotlock->node, &write_acquired);
    mutex_unlock(&rotlock_mutex);

    return 0;
}

SYSCALL_DEFINE2(rotunlock_read, int, degree, int, range)
{
    int cnt;

    if(degree < 0 || degree >= 360) {
  	    printk(KERN_ERR "degree should be 0 <= degree < 360\n");
        return -1;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -1;
    }

    mutex_lock(&rotlock_mutex);

    if(!list_empty(&write_acquired)) {
        printk(KERN_ERR "Writer is using. No readers found.\n");
        return -1;
    }

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
        return -1;
    }

    if(range <= 0 || range >= 180) {
        printk(KERN_ERR "range should be 0 < range < 180\n");
        return -1;
    }

    mutex_lock(&rotlock_mutex);

    if(!list_empty(&read_acquired)) {
        printk(KERN_ERR "Readers are using. No writer found.\n");
        return -1;
    }

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
