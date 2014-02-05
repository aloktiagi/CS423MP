#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */

#include "include/proc.h"
#include "include/usertime.h"

#define DRIVER_AUTHOR "Alok Tiagi"
#define DRIVER_DESC   "A sample driver"

static int __init cputime_measurer_initialize(void)
{
    printk(KERN_INFO "Initializing CPU time measurer\n");

    int status;


    status = proc_initialize();
    if (status)
    {
        return (status);
    }

    status = init_mytimer();
    if (status)
    {
        return (status);
    }
    return 0;
}

static void __exit cputime_measurer_finalize(void)
{
    printk(KERN_INFO "Goodbye, world 4\n");
    proc_finalize();
    stop_timer();
}

module_init(cputime_measurer_initialize);
module_exit(cputime_measurer_finalize);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);	/* Who wrote this module? */
MODULE_DESCRIPTION(DRIVER_DESC);	/* What does this module do */
