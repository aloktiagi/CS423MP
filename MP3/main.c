#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/cdev.h>

#include "include/proc.h"
#include "include/profiler.h"

#define DRIVER_AUTHOR "Alok Tiagi Cedar Pan"
#define DRIVER_DESC   "Profiler"

/* Initialize the kernel module */
static int __init profiler_initialize(void)
{
    int status;
    printk(KERN_INFO "Initializing the profiler\n");

    /* Initialize the proc file system */
    status = proc_initialize();
    if (status)
    {
        return (status);
    }
    /* Initialize the profiler*/
    status = init_profiler();
    if (status)
    {
        return (status);
    }
    return 0;
}

/* Exit the module */
static void __exit profiler_finalize(void)
{
    printk(KERN_INFO "Finalizing the module\n");
    /* remove all proc entries */
    proc_finalize();
    /* flush the work queue, cleanup the list
    and unregister the char device 
    */
    stop_profiler();
}

module_init(profiler_initialize);
module_exit(profiler_finalize);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(DRIVER_DESC);	/* What does this module do */
