#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>

#include "include/proc.h"

/** Base directory for proc files */
#define MODULE_DIR "mp1"

/** Directory entry. */
static struct proc_dir_entry *dir = NULL;


int proc_initialize(void)
{

    /* Create the directory. */
    dir = proc_mkdir(MODULE_DIR, NULL);
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
    remove_proc_entry(MODULE_DIR, NULL);
}

