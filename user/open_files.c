#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main() {
    int fd_1 = open("output.txt", O_WRONLY|O_CREATE);
    int fd_2 = open("output.txt", O_WRONLY|O_CREATE);
    printf("fd_1 is %d\n", fd_1);
    printf("fd_2 is %d\n", fd_2);
    write(fd_1, "hello,world\n",12);
    write(fd_2, "abc\n",4); // overwrite hell 
    exit(0);
}