#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/capability.h>
#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include<linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#include "include/usertime.h"
#include "include/mp2_given.h"

/* Timer structure */
static struct timer_list my_timer;
/* Work Queue structure */
static struct workqueue_struct* wq = NULL;

struct kmem_cache *task_cache = NULL;

enum task_state {
    SLEEPING,
    READY,
    RUNNING
};

/* Process info linked list */

typedef struct my_task {
    struct task_struct *linux_task;
    struct timer_list wakeup_timer;
    enum task_state state;
    unsigned int pid;
    unsigned int period;
    unsigned int computation;
    struct list_head task_node;
} my_task_t;

my_task_t *running_task;

LIST_HEAD(process_list);
static DEFINE_MUTEX(process_list_lock);

void timer_cb(my_task_t *task)
{
    printk("\n Timer expired for process pid %u",task->pid);
    initialize_timer(task);
    if(running_task != task)
    {
        task->state = READY;
        //Call context switch
    }
}

void initialize_timer(my_task_t *task)
{
    printk("\n Setting up the timer for pid %u",task->pid);
    setup_timer(&task->wakeup_timer,timer_cb,task);
    mod_timer(&task->wakeup_timer,jiffies + msecs_to_jiffies(task->period));
}

void register_task(unsigned int pid, unsigned int period, unsigned int computation)
{
    my_task_t *new_task;
    
    new_task = kmem_cache_alloc(task_cache, GFP_ATOMIC);
    if(!new_task)
	return -ENOMEM;
    
    new_task->linux_task = find_task_by_pid(pid);
    new-task->pid = pid;
    new_task->period = period;
    new_task->computation = computation;
    new_task->state = SLEEPING;

    mutex_lock(&process_list_lock);
    INIT_LIST_HEAD(&new_task->task_node);
    list_add_tail(&new_task->task_node, &process_list);
    mutex_unlock(&process_list_lock);

    initialize_timer(new_task);
}

/* Initialize the work queue and start the timer */
int kthread_init(void)
{
    int ret;
    /* Start the context switch thread */
    task_cache = kmem_cache_create("task_cache", sizeof(struct my_task),
                                   0, SLAB_HWCACHE_ALIGN, NULL);
    if(!task_cache)
	return -ENOMEM;

    printk("\nContext switch thread started....");
    return 0;

}
/* Clean up module. Delete the timer, clean up the 
   work queue and remove all entries from the list */
void kthread_stop(void)
{
    return;
} 


