#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/capability.h>
#include <linux/compiler.h>

#include "include/usertime.h"

static struct timer_list my_timer;

void update_user_data()
{
  printk( "Updating user process data\n");
  unsigned long next_timer = jiffies + msecs_to_jiffies(200);
  mod_timer( &my_timer, next_timer);
}

int init_mytimer(void)
{
  int ret;

  printk("Timer module installing\n");

  init_timer(&my_timer);
  my_timer.function = update_user_data;

  ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(200) );
  if (ret) printk("Error in mod_timer\n");

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


