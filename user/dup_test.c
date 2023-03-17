#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main() {
    int fd1 = open("dup_test.txt", O_CREATE|O_WRONLY);
    int fd2 = dup(fd1);
    printf("fd1 is %d\nfd2 is %d\n", fd1, fd2);

    write(fd1, "hello", 6);
    write(fd2, ", world\n", 9);
    exit(0);
}