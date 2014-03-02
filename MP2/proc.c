#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "include/proc.h"
#include "include/usertime.h"

/** Base directory for proc files */
#define MODULE_DIR "mp2"
#define PROCFS_MAX_SIZE PAGE_SIZE

/** Directory entry. */
static struct proc_dir_entry *dir = NULL;
static struct proc_dir_entry *proc_entry = NULL;
/* Buffer to copy data */
static char procfs_buffer[PROCFS_MAX_SIZE];
/* intialize the size to 0 */
static unsigned long procfs_buffer_size = 0; 

/* Callback function for all write commands
   to our proc file */
int write_proc_cb(struct file* file, const char __user*  buffer, unsigned long count, void* data)
{
    char *input;
    unsigned int pid, period, computation;
    char op;

    input = kmalloc(count, GFP_KERNEL);

    procfs_buffer_size =  count;
    if(copy_from_user(input, buffer, count)) {
        return -EFAULT;
    }
    /* Find out the operation to be performed */
    op = input[0];
    switch(op)
    {
        case 'R':
            sscanf(input+2, "%u,%u,%u", &pid, &period, &computation);
            /* register a new task with pid, period and its computation */
            if(!can_add_task(period, computation)) {
                printk("\n Cannot register task with pid %u Period %u computation %u", pid, period, computation);
                break;
            }
            register_task(pid, period, computation);
            printk(KERN_INFO "Register pid %u Period %u computation %u\n", pid, period, computation);
            break;
        case 'D':
            sscanf(input+2, "%u", &pid);
            /* Deregister a task */
            deregister_task(pid);
            break;
        case 'Y':
            sscanf(input+2, "%u", &pid);
            /* Yield task */
            yield_task(pid);
            break;
        default:
            printk(KERN_INFO "Error regstering process");

    }

    kfree(input);
    return(count);
}

/* call back function for all reads done on 
   our proc file */
int read_proc_cb(char* buffer, char** buffer_location, off_t offset, int buffer_length, int* eof, void* data)
{
    int ret;
    char *proc_buff = NULL;
    int numofdata;

    if (offset > 0) {
        ret  = 0;
    } else {
        get_tasks_from_list(&proc_buff);
        numofdata = sprintf(buffer, "%s", proc_buff);

        kfree(proc_buff);
        ret = numofdata;
    }
    return ret;
}

/* initialize the proc directory and create
   a file under it */
int proc_initialize(void)
{

    /* Create the directory. */
    dir = proc_mkdir(MODULE_DIR, NULL);

    proc_entry = create_proc_entry( "status", 0666, dir);

    proc_entry->read_proc =read_proc_cb;
    proc_entry->write_proc =write_proc_cb;
    proc_entry->mode = S_IFREG | S_IRUGO ;
    proc_entry->uid = 0;
    proc_entry->gid = 0;

    if (dir == NULL)
    {
        /* Something went wrong */
        return (-ENOENT);
    }

    return (0);
}

/* module exiting, remove the proc file and directory */
void proc_finalize(void)
{
    /* Remove the directory */
    remove_proc_entry("status", dir);
    remove_proc_entry(MODULE_DIR, NULL);
}

