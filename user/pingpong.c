#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#define MAXBUFSIZE 100
int main() {
    int p[2];
    pipe(p);  // p[0] => readend; p[1] => writeend 
    if (fork() == 0) {
        char bufchild[MAXBUFSIZE];
        read(p[0], bufchild, sizeof(bufchild)); // waiting for data to arrive 
        printf("%d: received %s\n",getpid(),bufchild);
        write(p[1], "pong",4);  // write to parent 
        close(p[1]);  // close writeend 
    }
    else {
        write(p[1], "ping", 4); // write to child
        close(p[1]); // close writeend 
        wait((int*)0);  // wait child to write data and exit 
        char bufparent[MAXBUFSIZE];
        read(p[0],bufparent, sizeof(bufparent));
        printf("%d: received %s\n",getpid(),bufparent); 
    }
    exit(0);
}