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

/* Memory buffer */
#define BUFF_SIZE 128*PAGE_SIZE

/* Work Queue structure */
static struct workqueue_struct* wq = NULL;

/* Pointer to memory buffer */
static unsigned long* profiler_buff;
/* Current location in the buffer */
static int curr_buff;
/* Char dev structures */
static dev_t dev_no;
static struct cdev cdevice_driver;

/* Process info linked list
    Cpu utilization
    minor faults
    major faults
     */
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
/* Work queue call back to be called when
work is scheduled */
static void work_queue_callback(void *task);

static DECLARE_DELAYED_WORK(d_wq, &work_queue_callback);

/* File operation structure */
struct file_operations fops ={
	.open=NULL,
	.release=NULL,
 	.mmap=dev_mmap,
    .owner = THIS_MODULE
};

/* Delete work queue */
void delete_work_queue(void)
{
    if(wq) {
        cancel_delayed_work(&d_wq);
        flush_workqueue(wq);
        destroy_workqueue(wq);
    }
    wq = NULL;
}

/* Create a workqueue */
void create_work_queue(void)
{
    if(!wq)
        wq = create_workqueue("my_work");
    queue_delayed_work(wq, &d_wq, msecs_to_jiffies(50));
}

/* Get all registered tasks */
int get_tasks_from_list(char **proc_buffer)
{
    int index = 0;
    task_struct *task;
    *proc_buffer = (char*) kmalloc(2048, GFP_KERNEL);
    *proc_buffer[0] = '\0';

    mutex_lock(&process_list_lock);
    list_for_each_entry(task, &process_list, list)
    {
        index += sprintf(*proc_buffer+index, "%d\n", task->pid);
    }
    mutex_unlock(&process_list_lock);
    return index;
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

/* Update processes with info, cpu utilization
minor faults and major faults */
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

/* Update the memory buffer with the values of
cpu utilization, minor and major faults */
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

/* Deregister a process */
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
    /* Delete work queue if it was the last process */
    mutex_unlock(&process_list_lock);
    if(list_empty(&process_list)) {
        printk(KERN_INFO "\n Last process removing work queue");
        delete_work_queue();
    }
    return 0;
}

/* register a new process */
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

    /* Start work queue if this is the first
    process
    */
    if(list_empty(&process_list) == 0) {
        printk("\n First process, schedule the job");
        create_work_queue();
    }

    return 0;
}
/*
int dev_open(struct inode* inode_ptr,struct file* file_ptr)
{
    return 0;
}

int dev_release(struct inode* inode_ptr,struct file* file_ptr)
{
    return 0;
}
*/

/* Char device mmap function */
int dev_mmap(struct file* file_ptr, struct vm_area_struct* vm_area)
{
    unsigned long size=vm_area->vm_end - vm_area->vm_start;
    
    int ret=-1;
    int i = 0;
    printk(KERN_INFO "character device driver map.\n");
    /* Loop through with increments of page size to map 
    virtual page to the physical page */
    for(i = 0; i < size; i += PAGE_SIZE) {
        //physical_page = vmalloc_to_pfn(profiler_buff);
    
        ret=remap_pfn_range(vm_area,
                            vm_area->vm_start + i,
                            vmalloc_to_pfn((void*)(((unsigned long) profiler_buff) + i )), 
                            PAGE_SIZE, 
                            vm_area->vm_page_prot);
  
        if(ret < 0)
        {
            printk(KERN_INFO "error occurs at remap_pfn_range.\n");
            return -EAGAIN;
        }
    }
    return 0;
}



/* Initialize the memory buffer and create the char device */
int init_profiler(void)
{
    int ret = 0;

    profiler_buff = (unsigned long*) vmalloc(BUFF_SIZE);
    if(!profiler_buff)
        return -ENOMEM;
    memset ((void*)profiler_buff, 0, BUFF_SIZE);
    curr_buff = 0;

    int status=-1;
    status=alloc_chrdev_region(&dev_no, 0, 1, "mp3_char_device_driver");
    
    if(status<0)
    {
       printk(KERN_INFO "Major number allocation failed.\n");
       return status;
    }

   cdev_init(&cdevice_driver,&fops);
   cdevice_driver.owner = THIS_MODULE;
   cdevice_driver.ops = &fops;
   status=cdev_add(&cdevice_driver, dev_no, 1);
   
   if(status<0){
     printk(KERN_INFO "Char device driver register failed. \n");
   }
   else{
     printk(KERN_INFO "Char device driver registered. \n");
   }
   
   return ret;
}

/* Clean up module.Delete the work queue, remove 
all processes from the list, free the buffer and unregister
the char device
 */
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


