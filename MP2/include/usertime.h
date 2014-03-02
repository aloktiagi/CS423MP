#ifndef USERTIME_HEADER
#define USERTIME_HEADER

/* Task states */

enum task_state {
    SLEEPING,
    READY,
    RUNNING
};

/* Task struct */

typedef struct my_task {
    struct task_struct *linux_task;
    struct timer_list wakeup_timer;
    enum task_state state;
    unsigned int pid;
    unsigned int period;
    unsigned int computation;
    struct list_head task_node;
    unsigned int next_period;
} my_task_t;

/**
 * Initialize kthread
 */
extern int kthread_init(void);

/**
 * Stop kthread
 */
extern void stop_kthread(void);

/**
  * Register a new task
  */
extern void register_task(unsigned int pid,
                          unsigned int period,
                          unsigned int computation);

/* Dispatch thread function */
extern int dispatching_thread_function(void * nothing); 

/* get all registered tasks */
extern unsigned int get_tasks_from_list(char **tasks);

/* Yield task */
extern void yield_task(unsigned int pid);

/* De register a task */
void deregister_task(unsigned long pid);

/* Admission control */
extern bool can_add_task(unsigned int period, unsigned int computation);

#endif 

