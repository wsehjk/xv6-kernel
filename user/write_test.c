#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
int main(int argc, char* argv) {
    int fd = open("test.txt", O_CREATE|O_WRONLY);
    if (fork() == 0) {
        char *name = "hello, world";
        write(fd, name, sizeof(name));
        exit(0);
    } else {
        char *name = "abcjiand";
        write(fd, name, sizeof(name));
        wait(0);
        exit(0);
    }
    return 0;
}