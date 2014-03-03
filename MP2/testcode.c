#include<stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>

#define PROC_FILENAME "/proc/mp2/status"

long int factorial(long int no)
{
    if(no < 1) return 1;
    return no*factorial(no-1);
}

double time_diff(struct timeval *prev,
		 struct timeval *curr)
{
	double diff;

        diff = (curr->tv_sec*1000.0 + curr->tv_usec/1000.0) -
                (prev->tv_sec*1000.0 + prev->tv_usec/1000.0);

        return diff;
}

int main(int argc, char **argv)
{
    char cmd[120];
    pid_t mypid;
    int j,k;
    char line[120];
    FILE *file;
    unsigned long int pid, period, computation;
    struct timeval t0;

    unsigned long int myperiod;
    int jobs;

    myperiod = atoi(argv[1]);
    jobs = atoi(argv[2]);

    mypid = getpid();
    printf("\nMy pid %d",mypid);
    printf("\n Registering");
    sprintf(cmd, "echo 'R,%d,%lu,100'>" PROC_FILENAME, mypid,myperiod);
    system(cmd);

    file = fopen(PROC_FILENAME, "r");
    while(fgets(line, sizeof(line), file) != NULL)
    {
        sscanf(line, "%lu: %lu %lu\n", &pid, &period, &computation);
        if(pid == mypid)
            break;
    }
    fclose(file);

    if(pid != mypid)
    {
        printf("Could not schedule our task with pid %d\n",mypid);
        return -1;
    }

    sprintf(cmd, "echo 'Y,%d'>" PROC_FILENAME, mypid);
    gettimeofday(&t0, NULL);
    system(cmd);

    printf("\n Yield--");
    while(jobs > 0)
    {
        struct timeval start_time, end_time;
        long int result;
        int i;
        long int n = 6000;
        gettimeofday(&start_time,NULL);
         
        printf("\nWakeup time is  %lf msecs",time_diff(&t0,&start_time));

        for(i=0;i<n;i++) {
            result = factorial(i);
        }
        gettimeofday(&end_time,NULL);
        printf("\nComputation time is  %lf msecs",time_diff(&t0,&end_time));

        sprintf(cmd, "echo 'Y, %d'>" PROC_FILENAME, mypid);
        system(cmd);
        jobs--;
        printf("\n Job done");
    }

    sprintf(cmd, "echo 'D, %d'>" PROC_FILENAME, mypid);
    system(cmd);
    printf("\n De-registering");

    return 0;
}
    
