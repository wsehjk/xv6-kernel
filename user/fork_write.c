#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
int main() {
    int pid = fork();
    if (pid == 0) {
        close(1);
        open("output.txt", O_WRONLY|O_CREATE);
        write(1,"child\n", 6);  // to output.txt files 
        //printf("child\n");
    }else {
        write(1,"parent\n",7);  // to terminal output 
        //printf("parent\n");
    }
    exit(0);
}