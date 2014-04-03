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
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>


#include "include/profiler.h"
#include "include/mp3_given.h"

#define BUFF_SIZE 128*4*1000

/* Work Queue structure */
static struct workqueue_struct* wq = NULL;

static unsigned long* profiler_buff;
static int curr_buff;
static dev_t dev_no;
static struct cdev cdevice_driver;

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

static void work_queue_callback(void *task);

static DECLARE_DELAYED_WORK(d_wq, &work_queue_callback);

void delete_work_queue(void)
{
    if(wq) {
        cancel_delayed_work(&d_wq);
        flush_workqueue(wq);
        destroy_workqueue(wq);
    }
    wq = NULL;
}

void create_work_queue(void)
{
    if(!wq)
        wq = create_workqueue("my_work");
    queue_delayed_work(wq, &d_wq, msecs_to_jiffies(50));
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
    unsigned long majorflts = 0;
    unsigned long minorflts = 0;
    unsigned long util = 0;

    mutex_lock(&process_list_lock);
    list_for_each_entry_safe(curr, next, &process_list, list)
    {
        if(update_data(curr) == 0) {
            majorflts += curr->major_fault;
            minorflts += curr->minor_fault;
            util += curr->utilization;            
        }
    }
    mutex_unlock(&process_list_lock);
    profiler_buff[curr_buff++] = jiffies;   
    profiler_buff[curr_buff++] = majorflts;
    profiler_buff[curr_buff++] = minorflts;
    profiler_buff[curr_buff++] = util;
    printk("\n maj %lu min %lu util %lu",majorflts, minorflts, util);
    if(curr_buff+4 >= BUFF_SIZE/sizeof(unsigned long)) {
        curr_buff = 0;
    }
}

void work_queue_callback(void *task)
{
    update_profiler_buffer();
    if(wq)
        queue_delayed_work(wq, &d_wq, msecs_to_jiffies(50));
    printk(KERN_INFO "\n Work queue call back");
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
        }
    }
    mutex_unlock(&process_list_lock);
    if(list_empty(&process_list)) {
        printk(KERN_INFO "\n Last process removing work queue");
        delete_work_queue();
    }
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
        create_work_queue();
    }

    return 0;
}

int dev_open(struct inode* inode_ptr,struct file* file_ptr)
{
    return 0;
}

int dev_release(struct inode* inode_ptr,struct file* file_ptr)
{
    return 0;
}

int dev_mmap(struct file* file_ptr, struct vm_area_struct* vm_area)
{
    unsigned long physical_page;
    unsigned long size=vm_area->vm_end - vm_area->vm_start;
    
    int ret=-1;
    printk(KERN_INFO "character device driver map.\n");
    
    physical_page = vmalloc_to_pfn(profiler_buff);
    
    ret=remap_pfn_range(vm_area, vm_area->vm_start, physical_page, size, vm_area->vm_page_prot);

    if(ret)
    {
       printk(KERN_INFO "error occurs at remap_pfn_range.\n");
       return -EAGAIN;
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

    struct file_operations fops ={
	.open=dev_open,
	.release=dev_release,
 	.mmap=dev_mmap
    };

    int status=-1;
    int major=-1;
    status=alloc_chrdev_region(&dev_no, 0, 1, "mp3_char_device_driver");
    
    if(status<0)
    {
       printk(KERN_INFO "Major number allocation failed.\n");
       return status;
    }

   major=MAJOR(dev_no);
   dev_no=MKDEV(major,0);
   
   cdev_init(&cdevice_driver,&fops);
   status=cdev_add(&cdevice_driver, dev_no, 1);
   
   if(status<0){
     printk(KERN_INFO "Char device driver register failed. \n");
   }
   else{
     printk(KERN_INFO "Char device driver registered. \n");
   }
   
   return ret;
}

/* Clean up module. Delete the timer, clean up the 
   work queue and remove all entries from the list */
void stop_profiler(void)
{
    if(wq)
        delete_work_queue();
    clean_up_list();
    if(profiler_buff)
        vfree(profiler_buff);
    
    /*clean up device driver*/
    cdev_del(&cdevice_driver);
    unregister_chrdev_region(dev_no,1);
} 


