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

my_task_t *running_task;

LIST_HEAD(process_list);
static DEFINE_MUTEX(process_list_lock);

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

void timer_cb(my_task_t *task)
{
    printk("\n Timer expired for process pid %u",task->pid);
    initialize_timer(task);
    if(running_task != task)
    {
        task->state = READY;
        //wakeupthread
    }
}

void initialize_timer(my_task_t *task)
{
    printk("\n Setting up the timer for pid %u",task->pid);
    setup_timer(&task->wakeup_timer,timer_cb,task);
    mod_timer(&task->wakeup_timer,jiffies + msecs_to_jiffies(task->period));
}

void yield_task(unsigned int pid)
{
    unsigned int release_time;
    my_task_t *task;
    if(running_task && (running_task->pid == pid)) {
        task = running_task;
    }
    else {
        task = find_my_task_by_pid(pid);
    }
    if(jiffies < task->next_period) {
        release_time = jiffies + task->next_period;
        task->state = SLEEPING;
        mod_timer(&task->wakeup_timer,jiffies + release_time);
        running_task = NULL;
        //wakeupthread
    }
    else {
        if(task->state == SLEEPING)
            task->state = READY;
        //wakeupthread
        running_task = NULL;
    }
    set_task_state(task->linux_task, TASK_UNINTERRUPTIBLE);
}

void register_task(unsigned int pid, unsigned int period, unsigned int computation)
{
    my_task_t *new_task;
    
    new_task = kmem_cache_alloc(task_cache, GFP_ATOMIC);
    if(!new_task)
	return -ENOMEM;
    
    //new_task->linux_task = find_task_by_pid(pid);
    new_task->pid = pid;
    new_task->period = period;
    new_task->computation = computation;
    new_task->state = SLEEPING;

    mutex_lock(&process_list_lock);
    INIT_LIST_HEAD(&new_task->task_node);
    list_add_tail(&new_task->task_node, &process_list);
    mutex_unlock(&process_list_lock);

//    initialize_timer(new_task);
    setup_timer(&new_task->wakeup_timer,timer_cb,new_task);
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
    printk(KERN_DEBUG "Context switch thread\n");
    kthread_run(dispatching_thread_function,NULL,"MP2_dispatching_thread");
	

    return 0;

}
/* Clean up module. Delete the timer, clean up the 
   work queue and remove all entries from the list */
/*
void kthread_stop(void)
{
    clean_up_list();
    kmem_cache_destroy(task_cache);
    return;
} 

*/

/**
 *create and run thread
 * 	k_thread_run(dispatching_thread_function,NULL,"MP2_dispatching thread");
 *
 */


//thread function for dispatching thread

int dispatching_thread_function(void *nothing)
{
	my_task_t *new_task;

	while(1)
	{
		//check if need to stop the thread

		if(kthread_should_stop())
			kthread_stop();

		//unlock the linked list
		mutex_unlock(&process_list_lock);
		//get the new task: the one in ready state with the highest priority

		if(!list_empty(&process_list)) //check that the list is not empty
		{
			my_task_t * curr;
			my_task_t * next;
			

			//iterate the task list to find 
			list_for_each_entry_safe(curr,next, &process_list, task_node)
			{
				if( curr->state==READY  && (curr->period < new_task->period))
					new_task=curr;  
			}	  

			//lock the linked list
			mutex_lock(&process_list_lock);

			//compare with current task

			if(running_task!=NULL) //check if the running task is empty
			{

				if(new_task->period < running_task->period)
				{
					//set the task to be -interruptible
				   set_task_state(running_task->linux_task,TASK_INTERRUPTIBLE);
					//give up the control to scheduler
				   struct sched_param sparam_old;
				   sparam_old.sched_priority=0;
				   sched_setscheduler(running_task->pid,SCHED_NORMAL, &sparam_old);
				}

				struct sched_param sparam_new;
				wake_up_process(new_task->linux_task);
				sparam_new.sched_priority=MAX_USER_RT_PRIO-1;
				sched_setscheduler(new_task->pid,SCHED_FIFO,&sparam_new);	
				running_task=new_task;
				running_task->state=RUNNING;

			}
			else
			{
				printk(KERN_INFO "Current running task has highest priority");
				continue;
			}

		
		}
	}
	
	return 0;

}
