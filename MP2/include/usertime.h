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

extern int dispatching_thread_function(void * nothing); 


#endif 

