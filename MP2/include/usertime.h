#ifndef USERTIME_HEADER
#define USERTIME_HEADER

/**
 * Initialize timer.
 */
extern int kthread_init(void);

/**
 * Stop timer.
 */
extern void kthread_stop(void);

/**
  * Register a new process 
  */
extern void register_task(unsigned int pid,
                          unsigned int period,
                          unsigned int computation);

extern unsigned int get_tasks_from_list(char **tasks);

extern void yield_task(unsigned int pid);

#endif 

