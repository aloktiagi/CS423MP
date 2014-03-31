#ifndef PROFILER_HEADER
#define PROFILER_HEADER

/**
 * Initialize timer.
 */
extern int init_profiler(void);

/**
 * Stop Profiler.
 */
extern void stop_profiler(void);

extern int register_process(pid_t pid);

extern int deregister_process(pid_t pid);

/**
 * Return all process ids and their cputimes.
 */
unsigned int get_process_times_from_list(char **process_times);

#endif /* !PROFILER_HEADER */

