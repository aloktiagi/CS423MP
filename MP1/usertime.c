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

#include "include/usertime.h"
#include "include/mp1_given.h"

static struct timer_list my_timer;
static struct workqueue_struct* wq = NULL;

typedef struct user_process_info {
  struct list_head list;
  pid_t pid;
  unsigned long timestamp;
} process_info;

LIST_HEAD(process_list);

void add_process_to_list(pid_t pid)
{
    process_info *new = (process_info *) kmalloc( sizeof(process_info), GFP_KERNEL );
    new->pid = pid;
    printk( "Updating list with pid %d\n",pid);
    new->timestamp = 0;
    list_add( &new->list, &process_list);
}

unsigned int get_process_times_from_list(char **process_times)
{
    unsigned int index = 0;
    process_info *pid_data;

    *process_times = (char *)kmalloc(2048, GFP_KERNEL);
    *process_times[0] = '\0';

    list_for_each_entry(pid_data, &process_list, list)
    {
        index += sprintf(*process_times, "%s%d: %lu\n", *process_times,pid_data->pid, pid_data->timestamp);
    }
    return index;
}

void cpu_time_update()
{
    process_info *pid_data;
    list_for_each_entry(pid_data, &process_list, list)
    {
        int result;
        unsigned long cputime;
        result = get_cpu_use(pid_data->pid,&cputime);
        if(result < 0) {
            printk("\nCould not update cputime, removing process");
        }
        else {
            printk(KERN_INFO "\n id %d with time %lu",pid_data->pid,pid_data->timestamp);
            pid_data->timestamp = cputime;
        }
    }
}

void work_function_cb(struct work_struct *work)
{
    cpu_time_update();
    kfree( (void *)work );
    return;
}

void add_to_work_queue()
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

void update_user_data()
{
  unsigned long next_timer = jiffies + msecs_to_jiffies(5000);
  printk( "Updating user process data\n");
  add_to_work_queue();
  mod_timer( &my_timer, next_timer);
}

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

void stop_timer(void)
{
  int ret;

  ret = del_timer( &my_timer );
  if (ret) printk("The timer is still in use...\n");

  printk("Timer module uninstalling\n");

  return;
}


