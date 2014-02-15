#include<stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>

long int factorial(int no)
{
    if(no < 1) return 1;
    return no*factorial(no-1);
}
int main()
{
    pid_t pid = getpid();
    long int result;
    int i;
    long int bigno = 100000;
    char *proc_entry = NULL;
    char *read_entry = NULL;
    asprintf(&proc_entry, "echo %d > /proc/mp1/status",pid);
    system(proc_entry);
    for(i=0;i<bigno;i++) {
        result = factorial(i);
    }
    asprintf(&read_entry,"cat /proc/mp1/status");
    system(read_entry);
    free(proc_entry);
    free(read_entry);
    return 0;
}
    
