#include<stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>

#define PROC_FILENAME "/proc/mp1/status"

long int factorial(int no)
{
    if(no < 1) return 1;
    return no*factorial(no-1);
}
int main(int argc, char **argv)
{
    char cmd[120];
    pid_t mypid;
    int j,k;
    char line[120];
    FILE *file;
    unsigned long int pid, period, computation;
    struct timeval last_tv, current_tv;

    mypid = getpid();
    sprintf(cmd, "echo 'R,%lu,500,10'>" PROC_FILENAME, mypid);
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
        printf("Could not schedule our task\n");
        return -1;
    }

    sprintf(cmd, "echo 'Y,%lu'>" PROC_FILENAME, mypid);
    gettimeofday(&last_tv, NULL);
    system(cmd);

    for(k = 0; k < 2; k++)
        for(j = 0; j < 10; j++)
        {
            gettimeofday(&current_tv, NULL);
            printf("% 4ld sec % 5ld us | fact(%u): %llu\n",
                    current_tv.tv_sec - last_tv.tv_sec,
                    current_tv.tv_usec - last_tv.tv_usec,
                    j, factorial(j));
            last_tv = current_tv;

            sprintf(cmd, "echo 'Y, %lu'>" PROC_FILENAME, mypid);
            system(cmd);
        }

    sprintf(cmd, "echo 'D, %lu'>" PROC_FILENAME, mypid);
    system(cmd);

    return 0;
}
    
