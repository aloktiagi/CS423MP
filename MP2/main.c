#include <linux/module.h>	
#include <linux/kernel.h>	
#include <linux/init.h>

#include "include/proc.h"
#include "include/usertime.h"

#define DRIVER_AUTHOR "Alok Tiagi Cedar Pan"
#define DRIVER_DESC   "Rate Monotonic Scheduler"

/* Initialize the kernel module */
static int __init rate_monotonic_scheduler_initialize(void)
{
    int status;
    printk(KERN_INFO "Initializing CPU time measurer\n");

    /* Initialize the proc file system */
    status = proc_initialize();
    if (status)
    {
        return (status);
    }
    /* Start the kthread and create task cache */
    status = kthread_init();
    if (status)
    {
        return (status);
    }
    return 0;
}

/* Exit the module */
static void __exit rate_monotonic_scheduler_finalize(void)
{
    printk(KERN_INFO "Finalizing the module\n");
    /* remove all proc entries */
    proc_finalize();
    /* Stop kthread, remove tasks and destroy cache */
    stop_kthread();
}

module_init(rate_monotonic_scheduler_initialize);
module_exit(rate_monotonic_scheduler_finalize);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
