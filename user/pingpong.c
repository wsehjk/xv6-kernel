#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// parent process and child process communicate via pipe
int main() {
    int parent_child[2];
    pipe(parent_child);
    int child_parent[2];
    pipe(child_parent);
    char buf[100];
    int a = fork();
    if (a < 0) {
        printf("fork error\n");
        exit(-1);
    } else if (a == 0) {    // child read ping, write pong
        // child read ping
        int id = getpid();
        // close(0);
        // dup(parent_child[0]);
        close(parent_child[1]);
        read(parent_child[0], buf, 100);
        close(parent_child[0]);
        printf("%d: received %s\n", id, buf);
        // child write pong
        close(child_parent[0]);
        write(child_parent[1], "pong", 4);
        close(child_parent[1]);  
    } else {    // parent write ping, read pong
        int id = getpid();
        // parent write ping,
        close(parent_child[0]);
        write(parent_child[1], "ping", 4);
        close(parent_child[1]); 
        // parent read pong 

        // close(0);
        // dup(child_parent[0]);
        close(child_parent[1]);        
        read(child_parent[0], buf, 100);
        close(child_parent[0]);        
        printf("%d: received %s\n", id, buf);
    }
    exit(0);
}