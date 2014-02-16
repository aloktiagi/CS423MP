#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */

#include "include/proc.h"
#include "include/usertime.h"

#define DRIVER_AUTHOR "Alok Tiagi Cedar Pan"
#define DRIVER_DESC   "CPU Time Measurer"

/* Initialize the kernel module */
static int __init cputime_measurer_initialize(void)
{
    int status;
    printk(KERN_INFO "Initializing CPU time measurer\n");

    /* Initialize the proc file system */
    status = proc_initialize();
    if (status)
    {
        return (status);
    }
    /* Start the work queue and the timer */
    status = init_mytimer();
    if (status)
    {
        return (status);
    }
    return 0;
}

/* Exit the module */
static void __exit cputime_measurer_finalize(void)
{
    printk(KERN_INFO "Finalizing the module\n");
    /* remove all proc entries */
    proc_finalize();
    /* flush the work queue, stop the timer 
       and clean up the list */
    stop_timer();
}

module_init(cputime_measurer_initialize);
module_exit(cputime_measurer_finalize);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(DRIVER_DESC);	/* What does this module do */
