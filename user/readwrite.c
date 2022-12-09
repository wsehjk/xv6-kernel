#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/param.h"
// void iota(char* buf, int a) {
//     `
// }
int main() {
    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(p[1]);
        int data = 0;
        read(p[0], &data, sizeof(int));
        printf("data is %d\n", data);
        close(p[0]);
    }
    else {
        int data = 50;
        close(p[0]);
        write(p[1], &data, sizeof(int));
        close(p[1]);
        wait((int*)0);
    }
    exit(0);
}