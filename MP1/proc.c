#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "include/proc.h"
#include "include/usertime.h"

/** Base directory for proc files */
#define MODULE_DIR "mp1"
#define PROCFS_MAX_SIZE PAGE_SIZE

/** Directory entry. */
static struct proc_dir_entry *dir = NULL;
static struct proc_dir_entry *proc_entry = NULL;

static char procfs_buffer[PROCFS_MAX_SIZE];

static unsigned long procfs_buffer_size = 0; 


int write_proc_cb(struct file* file, const char __user*  buffer, unsigned long count, void* data)
{
    int pid;
    procfs_buffer_size =  count;
    if(copy_from_user(procfs_buffer, buffer, procfs_buffer_size)) {
        return -EFAULT;
    }
    pid = simple_strtol(procfs_buffer, NULL, 10);
    add_process_to_list(pid);
    return procfs_buffer_size;

}

int read_proc_cb(char* buffer, char** buffer_location, off_t offset, int buffer_length, int* eof, void* data)
{
    int ret;
    char *proc_buff = NULL;
    int numofdata;

    if (offset > 0) {
        ret  = 0;
    } else {
        get_process_times_from_list(&proc_buff);
        numofdata = sprintf(buffer, "%s", proc_buff);

        kfree(proc_buff);
        ret = numofdata;
    }
    return ret;
}
int proc_initialize(void)
{

    /* Create the directory. */
    dir = proc_mkdir(MODULE_DIR, NULL);

    proc_entry = create_proc_entry( "status", 0644, dir);
    //proc_buffer = (char*)kmalloc(sizeof(char), GFP_KERNEL);

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

void proc_finalize(void)
{
    /* Remove the directory */
    remove_proc_entry("status", dir);
    remove_proc_entry(MODULE_DIR, NULL);
}

