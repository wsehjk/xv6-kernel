#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"
#define EXCLUDE
int main() {
    close(1);
    open("forkexec.txt",O_WRONLY|O_CREATE);
    write(1, "before fork\n", 12);
    #ifdef EXCLUDE
    if (fork() ==0) {
        char* argv[] = {"echo", "child",0};
        exec("echo",argv);
        printf("exec failed\n");
        exit(1);
    }
    else {
        int status;
        int child_pid = wait(&status);
        printf("parent: child %d returned with %d\n", child_pid, status);
    }
    #endif
    
    #ifndef EXCLUDE
    char* argv[] = {"echo", "child",0};
    exec("echo",argv);
    printf("exec failed\n");
    #endif
    exit(0);
}