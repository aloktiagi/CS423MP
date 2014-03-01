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
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include "include/usertime.h"
#include "include/mp2_given.h"

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
    unsigned int next_period;
} my_task_t;

my_task_t *running_task = NULL;

static struct task_struct *dispatch_kthread;

int stop_dispatch_kthread = 0;

LIST_HEAD(process_list);
static DEFINE_MUTEX(process_list_lock);
spinlock_t mytask_lock;
unsigned long flags;

void initialize_timer(my_task_t *task);

void clean_up_list()
{
    my_task_t *curr, *next;
    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, task_node)
    {
	list_del(&curr->task_node);
        del_timer(&curr->wakeup_timer);
        kmem_cache_free(task_cache, curr);
    }
    mutex_unlock(&process_list_lock);
}

unsigned int get_tasks_from_list(char **tasks)
{
    unsigned int index = 0;
    my_task_t *curr, *next;

    *tasks = (char *)kmalloc(2048, GFP_KERNEL);
    *tasks[0] = '\0';
    index += sprintf(*tasks+index, "PID : Period : Computation : State\n");
    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, task_node)
    {
        index += sprintf(*tasks+index, "%u:%u:%u:%d\n", curr->pid, curr->period, curr->computation, curr->state);
    }
    mutex_unlock(&process_list_lock);
    return index;

}

struct my_task *find_my_task_by_pid(unsigned int pid)
{
    my_task_t *curr, *next;
    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, task_node)
    {
        if(curr->pid == pid)
	    break;
    }
    mutex_unlock(&process_list_lock);
    return curr;
}

struct my_task *get_next_task(void)
{
    struct list_head *pos;
    my_task_t *curr, *next, *next_task = NULL;
    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, task_node)
    {
        if((curr->state == READY || curr->state == RUNNING) && 
           (next_task == NULL || curr->period < next_task->period)) {
            next_task = curr;
            printk(KERN_INFO "-------CUrr pointing to pid %u\n",curr->pid);
        }
    }
    mutex_unlock(&process_list_lock);
    //printk(KERN_INFO "Returning task for pid %u\n",next_task->pid);
    return next_task;

}

void timer_cb(my_task_t *task)
{
    //printk(KERN_INFO "Timer expired for pid %u\n",task->pid);
    //initialize_timer(task);
    spin_lock_irqsave(&mytask_lock, flags);
    task->state = READY;
    spin_unlock_irqrestore(&mytask_lock, flags);
    wake_up_process(dispatch_kthread);
}

void initialize_timer(my_task_t *task)
{
    //printk("\n Setting up the timer for pid %u",task->pid);
    mod_timer(&task->wakeup_timer,jiffies + msecs_to_jiffies(task->period));
}

bool can_add_task(unsigned int period, unsigned int computation)
{
    my_task_t *curr, *next;
    unsigned long utilization = (computation * 1000)/period;
    
    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, task_node)
    {
        utilization += ((curr->computation)*1000)/curr->period;
    }
    mutex_unlock(&process_list_lock);
    
    if(utilization > 693)
	return false;
    return true;
}

void deregister_task(unsigned long pid)
{
    my_task_t *task;
    struct sched_param sparam;
    task = find_my_task_by_pid(pid);
    del_timer_sync(&task->wakeup_timer);

    mutex_lock(&process_list_lock);
    list_del(&task->task_node);
    mutex_unlock(&process_list_lock);

    spin_lock_irqsave(&mytask_lock, flags);
    if(task == running_task)
        running_task = NULL;
    spin_unlock_irqrestore(&mytask_lock, flags);
    sparam.sched_priority = 0;
    sched_setscheduler(task->linux_task, SCHED_NORMAL, &sparam);
    kmem_cache_free(task_cache, task);
    wake_up_process(dispatch_kthread);
}

void yield_task(unsigned int pid)
{
    unsigned int release_time;
    my_task_t *task;
    struct sched_param sparam;
    if(running_task && (running_task->pid == pid)) {
        task = running_task;
    }
    else {
        task = find_my_task_by_pid(pid);
    }
    if(jiffies < task->next_period) {
        release_time = task->next_period - jiffies;

        spin_lock_irqsave(&mytask_lock, flags);
        task->state = SLEEPING;
        spin_unlock_irqrestore(&mytask_lock, flags);

        mod_timer(&task->wakeup_timer,jiffies + release_time);
        //printk(KERN_INFO "Putting task to sleep %u\n", pid);
        if(running_task && (running_task->pid == task->pid))
            running_task = NULL;
        //wakeupthread
    }
    else {
        //printk(KERN_INFO "Putting task to READY %u\n", pid);
        spin_lock_irqsave(&mytask_lock, flags);
        if(task->state == SLEEPING)
            task->state = READY;
        spin_unlock_irqrestore(&mytask_lock, flags);
        //wakeupthread
        running_task = NULL;
    }
    //printk(KERN_INFO "Yield for task with pid %u\n", pid);
    set_task_state(task->linux_task, TASK_UNINTERRUPTIBLE);
    sparam.sched_priority = 0;
    sched_setscheduler(task->linux_task, SCHED_NORMAL, &sparam);

    wake_up_process(dispatch_kthread);
}

void register_task(unsigned int pid, unsigned int period, unsigned int computation)
{
    my_task_t *new_task;
    
    new_task = kmem_cache_alloc(task_cache, GFP_ATOMIC);
    if(!new_task)
	return -ENOMEM;
    
    new_task->linux_task = find_task_by_pid(pid);
    new_task->pid = pid;
    new_task->period = period;
    new_task->computation = computation;
    new_task->state = SLEEPING;
    new_task->next_period = jiffies + msecs_to_jiffies(new_task->period);

    mutex_lock(&process_list_lock);
    list_add_tail(&new_task->task_node, &process_list);
    mutex_unlock(&process_list_lock);

    setup_timer(&new_task->wakeup_timer,timer_cb,new_task);
    printk(KERN_INFO "Registered new task with pid %u\n", pid);
}

//thread function for dispatching thread

int dispatching_kthread_function(void *nothing)
{
    my_task_t *new_task = NULL;

    printk(KERN_INFO "AIEEEEE dispatching_kthread_function called\n");
    while(1) {
        new_task = get_next_task();    
	if(stop_dispatch_kthread == 1)
	    break;

        spin_lock_irqsave(&mytask_lock, flags);
        if(!list_empty(&process_list)) {
	if(running_task != NULL) //check if the running task is empty
	{
            printk(KERN_INFO "Running task not null\n");
   	    if(new_task && (new_task->period < running_task->period))
	    {
		struct sched_param sparam_old;
	        //set the task to be -interruptible
	        //set_task_state(running_task->linux_task,TASK_INTERRUPTIBLE);
		//give up the control to scheduler
		sparam_old.sched_priority = 0;
                running_task->state = READY;
                printk(KERN_INFO "new task prio lower than running task %d\n",running_task->pid);
		sched_setscheduler(running_task->linux_task, SCHED_NORMAL, &sparam_old);
                running_task = NULL;
	    }
            else
            {
                printk(KERN_INFO "Current running task has highest priority\n");
                    continue;
            }
	}
        if(new_task != NULL) {
            printk(KERN_INFO "AIEEEEE new_task not null with pid %u\n",new_task->pid);
            struct sched_param sparam_new;
            wake_up_process(new_task->linux_task);
            sparam_new.sched_priority = MAX_USER_RT_PRIO-1;
            sched_setscheduler(new_task->linux_task, SCHED_FIFO, &sparam_new);
            running_task=new_task;
            running_task->state=RUNNING;
            running_task->next_period += msecs_to_jiffies(running_task->period);
        }
        }

        spin_unlock_irqrestore(&mytask_lock, flags);
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
        set_current_state(TASK_RUNNING);
    }
    return 0;
}

int kthread_init(void)
{
    struct sched_param sparam;

    /* Start the context switch thread */
    task_cache = kmem_cache_create("task_cache", sizeof(struct my_task),
                                   0, SLAB_HWCACHE_ALIGN, NULL);
    if(!task_cache)
        return -ENOMEM;
    printk(KERN_DEBUG "Context switch thread\n");

    running_task = NULL;

    dispatch_kthread = kthread_create(dispatching_kthread_function,
                                   NULL,
                                   "dispatching_kthread");

    return 0;

}

/* Clean up module. Delete the timer, clean up the
   work queue and remove all entries from the list */
void stop_kthread(void)
{
    stop_dispatch_kthread = 1;
    wake_up_process(dispatch_kthread);
    kthread_stop(dispatch_kthread);
    clean_up_list();
    kmem_cache_destroy(task_cache);
    return;
}

