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

#include "include/profiler.h"
#include "include/mp3_given.h"

#define BUFF_SIZE 128*4*1000

/* Work Queue structure */
static struct workqueue_struct* wq = NULL;
static struct delayed_work* d_wq = NULL;

static unsigned long* profiler_buff;
static int curr_buff;

/* Process info linked list */
typedef struct task_struct_t
{
    struct task_struct* linux_task;
    pid_t pid;
    unsigned long utilization;
    unsigned long minor_fault;
    unsigned long major_fault;
    struct list_head list;
}task_struct;

LIST_HEAD(process_list);
static DEFINE_MUTEX(process_list_lock);

static void work_callback(void *task);

void delete_work_queue(void)
{
    flush_workqueue(wq);
    destroy_workqueue(wq);
    wq = NULL;
}

/* Adds a newly registered process at 
   the end of the list */
void add_process_to_list(task_struct *task)
{
    mutex_lock(&process_list_lock);
    list_add_tail( &task->list, &process_list);
    mutex_unlock(&process_list_lock);
}

/* Clean up the list when the module exits */
void clean_up_list(void)
{
    task_struct *curr, *next;
    list_for_each_entry_safe(curr, next, &process_list, list)
    {
        mutex_lock(&process_list_lock);
        list_del(&curr->list);
        kfree(curr);
        mutex_unlock(&process_list_lock);
    }
}

int update_data(task_struct *task)
{
    unsigned long majorflts, minorflts, utime, stime;
    int ret = get_cpu_use(task->pid, &majorflts, &minorflts, &utime, &stime);
    if(ret == 0) {
        task->minor_fault = minorflts;
        task->major_fault = majorflts;
        task->utilization = utime + stime;
    }
    return ret;
}

void update_profiler_buffer(void)
{
    task_struct *curr, *next;
    unsigned long majorflts;
    unsigned long minorflts;
    unsigned long util;

    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, list)
    {
        if(update_data(curr) == 0) {
            majorflts += curr->major_fault;
            minorflts += curr->minor_fault;
            util += curr->utilization;            
        }
    }
    profiler_buff[curr_buff++] = jiffies;   
    profiler_buff[curr_buff++] = majorflts;
    profiler_buff[curr_buff++] = minorflts;
    profiler_buff[curr_buff++] = util;

    if(curr_buff+4 >= BUFF_SIZE/sizeof(unsigned long)) {
        curr_buff = 0;
    }
}

void schedule_job(void)
{
    INIT_DELAYED_WORK(wq, work_callback);
    queue_delayed_work(wq, d_wq, msecs_to_jiffies(50));
}

void work_callback(void *task)
{
    update_profiler_buffer();
    schedule_job();
}

int deregister_process(pid_t pid)
{
    task_struct *curr, *next;
    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, list)
    {
        if(curr->pid == pid) {
            list_del(&curr->list);
            kfree(curr);
            if(list_empty(&process_list) == 0) {
                delete_work_queue();
            }
        }
    }
    mutex_unlock(&process_list_lock);
    return 0;
}
int register_process(pid_t pid)
{
    task_struct *newtask;
    
    newtask = (task_struct*)kmalloc(sizeof(task_struct), GFP_KERNEL);
    newtask->pid = pid;
    newtask->linux_task = find_task_by_pid(pid);
    newtask->utilization = 0;
    newtask->minor_fault = 0;
    newtask->major_fault = 0;
    
    add_process_to_list(newtask);

    if(list_empty(&process_list) == 0) {
        printk("\n First process, schedule the job");
        schedule_job();
    }

    return 0;
}
/* Initialize the work queue and start the timer */
int init_profiler(void)
{
    int ret = 0;

    profiler_buff = (unsigned long*) vmalloc(BUFF_SIZE);
    if(!profiler_buff)
        return -ENOMEM;

    curr_buff = 0;
    
    d_wq = kmalloc(sizeof(struct delayed_work), GFP_ATOMIC);
    
    wq = create_workqueue("my_queue");
    if(!wq)
        return -ENOMEM;
  
    printk("\nWork Queue created");
    return ret;
}

/* Clean up module. Delete the timer, clean up the 
   work queue and remove all entries from the list */
void stop_profiler(void)
{
    if(!wq)
        delete_work_queue();
    clean_up_list();
    if(profiler_buff)
        vfree(profiler_buff);
} 


