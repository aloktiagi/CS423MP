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

/* Pointer to our cache */
struct kmem_cache *task_cache = NULL;
/* Pointer to running task */
my_task_t *running_task = NULL;
/* Flag to stop dispatch thread */
int stop_dispatch_kthread = 0;
/* Spinlock and its flags */
unsigned long flags;
spinlock_t mytask_lock;
/* Kthread struct */
static struct task_struct *dispatch_kthread;

/* Head of the task struct list */
LIST_HEAD(process_list);
/* Mutex lock for task list */
static DEFINE_MUTEX(process_list_lock);

/* Clean up the list: Delete entry
   Delete the timer and free cache */
void clean_up_list(void)
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

void timer_init(struct timer_list  *timer, void (*function)(unsigned long))
{
    init_timer(timer);
    timer->function = function;
}

/* Return tasks registered with the kernel module */
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

/* Return task from the pid given */
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

/* Get the next high priority task. The highest priority
   task can also be the current RUNNING task */
struct my_task *get_next_task(void)
{
    my_task_t *curr, *next, *next_task = NULL;
    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, task_node)
    {
        if((curr->state == READY || curr->state == RUNNING) && 
           (next_task == NULL || curr->period < next_task->period)) {
            next_task = curr;
        }
    }
    mutex_unlock(&process_list_lock);
    return next_task;

}

/* Timer callback for a task. Put task to ready state */
void timer_cb(unsigned long pid)
{
    /* Spin lock to protect task state */
    my_task_t *task;
    task = find_my_task_by_pid(pid);
    spin_lock_irqsave(&mytask_lock, flags);
    task->state = READY;
    spin_unlock_irqrestore(&mytask_lock, flags);
    wake_up_process(dispatch_kthread);
}

/* Admission control. Check if usage remains under 0.693 */
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

/* De register a task */
void deregister_task(unsigned long pid)
{
    my_task_t *task;
    struct sched_param sparam;
    /* Find task by pid */
    task = find_my_task_by_pid(pid);
    /* Delete tasks' timer */
    del_timer_sync(&task->wakeup_timer);
    /* Delete task from list */
    mutex_lock(&process_list_lock);
    list_del(&task->task_node);
    mutex_unlock(&process_list_lock);

    /* If same as running task, set running task to NULL */
    spin_lock_irqsave(&mytask_lock, flags);
    if(task == running_task)
        running_task = NULL;
    spin_unlock_irqrestore(&mytask_lock, flags);
    /* Reduce priority of task to NORMAL */
    sparam.sched_priority = 0;
    sched_setscheduler(task->linux_task, SCHED_NORMAL, &sparam);
    kmem_cache_free(task_cache, task);
    /* Wake up dispatcher thread */
    wake_up_process(dispatch_kthread);
}

/* Yield functionality */
void yield_task(unsigned int pid)
{
    unsigned int release_time;
    my_task_t *task;
    struct sched_param sparam;
    /* Check if it is is the running task
       else find it from the list */
    if(running_task && (running_task->pid == pid)) {
        task = running_task;
    }
    else {
        task = find_my_task_by_pid(pid);
    }
    /* If current time is less than next wake up period
    set the release time and put the task to SLEEPING state.
    Set the timer to expire at release time and mark running task as NULL 
    */
    if(jiffies < task->next_period) {
        release_time = task->next_period - jiffies;

        spin_lock_irqsave(&mytask_lock, flags);
        task->state = SLEEPING;
        spin_unlock_irqrestore(&mytask_lock, flags);
        task->wakeup_timer.expires = jiffies + release_time;
        mod_timer(&task->wakeup_timer,jiffies + release_time);
        if(running_task && (running_task->pid == task->pid))
            running_task = NULL;
    }
    /* If release period is passed, set task to ready state */
    else {
        spin_lock_irqsave(&mytask_lock, flags);
        if(task->state == SLEEPING)
            task->state = READY;
        spin_unlock_irqrestore(&mytask_lock, flags);
        running_task = NULL;
    }
    /* Set task state and wake up dispatcher thread */
    set_task_state(task->linux_task, TASK_UNINTERRUPTIBLE);
    sparam.sched_priority = 0;
    sched_setscheduler(task->linux_task, SCHED_NORMAL, &sparam);

    wake_up_process(dispatch_kthread);
}

/* Register a new task */
void register_task(unsigned int pid, unsigned int period, unsigned int computation)
{
    my_task_t *new_task;
    /* Allocate cache for task */
    new_task = kmem_cache_alloc(task_cache, GFP_ATOMIC);
    if(!new_task)
	return -ENOMEM;
    
    /* Point tasks struct to linux task struct*/
    new_task->linux_task = find_task_by_pid(pid);
    new_task->pid = pid;
    new_task->period = period;
    new_task->computation = computation;
    new_task->state = SLEEPING;
    new_task->next_period = jiffies + msecs_to_jiffies(new_task->period);

    init_timer(&new_task->wakeup_timer);
    new_task->wakeup_timer.function = timer_cb;
//    timer_init(&new_task->wakeup_timer,timer_cb);
    new_task->wakeup_timer.data = (unsigned long) pid;
  //  mod_timer(&new_task->wakeup_timer,jiffies +  msecs_to_jiffies(new_task->period));
    /* Add task to the task list */
    mutex_lock(&process_list_lock);
    list_add_tail(&new_task->task_node, &process_list);
    mutex_unlock(&process_list_lock);
    /* Set up the timer for the task */
    //setup_timer(&new_task->wakeup_timer,timer_cb,new_task);
    printk(KERN_INFO "Registered new task with pid %u\n", pid);
}

/* Dispatching thread implementation */
int dispatching_kthread_function(void *nothing)
{
    my_task_t *new_task = NULL;

    printk(KERN_INFO "AIEEEEE dispatching_kthread_function called\n");
    while(1) {
        /* Get the next highest priority task */
        new_task = get_next_task();    
        /* If stop thread, then exit */
	if(stop_dispatch_kthread == 1)
	    break;

        spin_lock_irqsave(&mytask_lock, flags);
        /* Check if task list is empty */
        if(!list_empty(&process_list)) {
	if(running_task != NULL) //check if there is a running task
	{
            // check if new task priority is higher than the current running tasks priority
   	    if(new_task && (new_task->period < running_task->period)) 
	    {
                // preempt the current running task, set its priority to normal and set running task to NULL
		struct sched_param sparam_old;
		sparam_old.sched_priority = 0;
                running_task->state = READY;
                printk(KERN_INFO "PREEMPTING RUNNING TASK WITH PID %d\n",running_task->pid);
		sched_setscheduler(running_task->linux_task, SCHED_NORMAL, &sparam_old);
                running_task = NULL;
	    }
            else
            {
                printk(KERN_INFO "Running Task has Highest Priority\n");
                    continue;
            }
	}
        if(new_task != NULL) {
            // Switch to higher priority task
            printk(KERN_INFO "Switching to higher priority task with pid %u\n",new_task->pid);
            struct sched_param sparam_new;
            // wake up the new task and set Real time priority
            wake_up_process(new_task->linux_task);
            sparam_new.sched_priority = MAX_USER_RT_PRIO-1;
            sched_setscheduler(new_task->linux_task, SCHED_FIFO, &sparam_new);
            // set running task to new task and set state to running
            running_task=new_task;
            running_task->state=RUNNING;
            // set the timer to expire at next period
            running_task->next_period += msecs_to_jiffies(running_task->period);
        }
        }

        spin_unlock_irqrestore(&mytask_lock, flags);
        // Set kthread to interruptible state, schedule it and set state to running
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
        set_current_state(TASK_RUNNING);
    }
    return 0;
}

/* INIT the scheduler */
int kthread_init(void)
{
    /* Allocate the cache */
    task_cache = kmem_cache_create("task_cache", sizeof(struct my_task),
                                   0, SLAB_HWCACHE_ALIGN, NULL);
    if(!task_cache)
        return -ENOMEM;
    /* Set running task to NULL */
    running_task = NULL;
    /* Start the dispatch thread */
    dispatch_kthread = kthread_create(dispatching_kthread_function,
                                      NULL,
                                      "dispatching_kthread");
    printk(KERN_DEBUG "Context switch thread\n");
    return 0;

}

/* Clean up module. Stop the dispatch thread,
   clean up the list and destroy the cache */
void stop_kthread(void)
{
    stop_dispatch_kthread = 1;
    wake_up_process(dispatch_kthread);
    kthread_stop(dispatch_kthread);
    clean_up_list();
    kmem_cache_destroy(task_cache);
    return;
}

