#ifndef USERTIME_HEADER
#define USERTIME_HEADER

/**
 * Initialize timer.
 *
 * @return Zero for success.
 */
extern int init_mytimer(void);

/**
 * Stop timer.
 */
extern void stop_timer(void);

void add_process_to_list(pid_t pid);

unsigned int get_process_times_from_list(char **process_times);

#endif /* !CPUM_HEADER */

