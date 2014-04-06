#ifndef PROFILER_HEADER
#define PROFILER_HEADER

/**
 * Initialize Profiler.
 */
extern int init_profiler(void);

/**
 * Stop Profiler.
 */
extern void stop_profiler(void);

/*
 * registetr a new process
 */
extern int register_process(pid_t pid);
/*
 * deregister a process
 */
extern int deregister_process(pid_t pid);

/**
 * Return all process ids.
 */
extern int get_tasks_from_list(char **process_times);

/*
 * Char device function callbacks
 */
extern int dev_open(struct inode* inode_ptr, struct file* file_ptr);
extern int dev_release(struct inode* inode_ptr, struct file* file_ptr);
extern int dev_mmap(struct file* file_ptr, struct vm_area_struct* vma_ptr);




#endif /* !PROFILER_HEADER */

