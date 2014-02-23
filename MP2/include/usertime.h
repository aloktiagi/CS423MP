#ifndef USERTIME_HEADER
#define USERTIME_HEADER

/**
 * Initialize timer.
 */
extern int init_mytimer(void);

/**
 * Stop timer.
 */
extern void stop_timer(void);

/**
 * Add a new registered process to the list.
 */
void add_process_to_list(pid_t pid);

/**
 * Return all process ids and their cputimes.
 */
unsigned int get_process_times_from_list(char **process_times);

#endif 

