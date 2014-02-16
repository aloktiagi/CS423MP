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
#include "include/mp1_given.h"

/* Timer structure */
static struct timer_list my_timer;
/* Work Queue structure */
static struct workqueue_struct* wq = NULL;
/* Process info linked list */
typedef struct user_process_info {
  struct list_head list;
  pid_t pid;
  unsigned long timestamp;
} process_info;

LIST_HEAD(process_list);
static DEFINE_MUTEX(process_list_lock);

/* Adds a newly registered process at 
   the end of the list */
void add_process_to_list(pid_t pid)
{
    process_info *new = (process_info *) kmalloc( sizeof(process_info), GFP_KERNEL );
    new->pid = pid;
    new->timestamp = 0;
    mutex_lock(&process_list_lock);
    list_add_tail( &new->list, &process_list);
    mutex_unlock(&process_list_lock);
}

/* Clean up the list when the module exits */
void clean_up_list(void)
{
    process_info *curr, *next;
    list_for_each_entry_safe(curr, next, &process_list, list)
    {
        mutex_lock(&process_list_lock);
        list_del(&curr->list);
        kfree(curr);
        mutex_unlock(&process_list_lock);
    }
}

/* Get all registered processes and return 
   them back to the user with pid and current
   cpu time */
unsigned int get_process_times_from_list(char **process_times)
{
    unsigned int index = 0;
    process_info *curr, *next;

    *process_times = (char *)kmalloc(2048, GFP_KERNEL);
    *process_times[0] = '\0';

    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, list)
    {
        index += sprintf(*process_times, "%s%d: %lu\n", *process_times,curr->pid, curr->timestamp);
    }
    mutex_unlock(&process_list_lock);
    return index;
}

/* Delete a process from list */
void delete_process_from_list(process_info *process)
{
    list_del(&process->list);
    kfree(process);
    return;
}

/* Update the current cpu time of a registered
   process when the work is scheduled */
void cpu_time_update(void)
{
    process_info *curr, *next;
    list_for_each_entry_safe(curr, next, &process_list, list)
    {
        int result;
        unsigned long cputime;
        result = get_cpu_use(curr->pid,&cputime);
        if(result < 0) {
            printk("\nCould not update cputime, removing process");
            delete_process_from_list(curr);
        }
        else {
            printk(KERN_INFO "\n id %d with time %lu", curr->pid, curr->timestamp);
            curr->timestamp = cputime;
        }
    }
}

/* Call back function when the work is 
   scheduled */
void work_function_cb(struct work_struct *work)
{
    mutex_lock(&process_list_lock);
    cpu_time_update();
    kfree( (void *)work );
    mutex_unlock(&process_list_lock);
    return;
}

/* Add work of cpu time update to the work queue */
void add_to_work_queue(void)
{
    struct work_struct* work = kmalloc(sizeof(struct work_struct), GFP_KERNEL);
    if(work) {
        INIT_WORK(work, work_function_cb);
        printk(KERN_INFO "INIT work done \n");
        if(!work)
        {
            printk(KERN_INFO "could not initialize this work \n");
            return;
        }
        queue_work(wq, work);
    }
}

/* Timer call back called when the timer value expires */
void update_user_data(unsigned long data)
{
    unsigned long next_timer = jiffies + msecs_to_jiffies(5000);
    printk( "\nUpdating user process data");
    add_to_work_queue();
    mod_timer( &my_timer, next_timer);
}

/* Initialize the work queue and start the timer */
int init_mytimer(void)
{
    int ret;
    wq = create_workqueue("my_queue");
    if(!wq) {
        printk("\n Could not create a work queue");
    }
    printk("\nWork Queue created");
  
    init_timer(&my_timer);
    my_timer.function = update_user_data;

    ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(5000));
    if (ret) printk("Error in mod_timer\n");

    printk("Timer installed \n");

    return 0;

}
/* Clean up module. Delete the timer, clean up the 
   work queue and remove all entries from the list */
void stop_timer(void)
{
    int ret;

    ret = del_timer( &my_timer );
    if (ret) printk("The timer is still in use...\n");
    printk("Timer removed\n");
    flush_workqueue(wq);
    destroy_workqueue(wq);
    clean_up_list();
    return;
} 


