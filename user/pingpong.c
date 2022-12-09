#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#define MAXBUFSIZE 100
/*int main() {
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
}*/

int main() {
    int f2c[2];
    int c2f[2];
    pipe(f2c);
    pipe(c2f);
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork error\n");
        close(f2c[0]);
        close(f2c[1]);
        close(c2f[0]);
        close(c2f[1]);
        exit(1);
    } 
    else if(pid == 0) {  // children 
       close(f2c[1]); 
       close(c2f[0]);
       char chilbbuf[MAXBUFSIZE];
       if (read(f2c[0], chilbbuf, MAXBUFSIZE) < 0) {
           fprintf(2, "child read error\n");
           exit(1);
       }
       printf("%d: received %s\n",getpid(),chilbbuf);
       close(f2c[0]);
       if (write(c2f[1], "pong", 4) != 4) {
          fprintf(2, "child write error\n");
          exit(1);
       }

       close(c2f[1]);
    }
    else {   // father 
        close(c2f[1]);
        close(f2c[0]);
        char parentbuf[MAXBUFSIZE];
        if (write(f2c[1], "ping", 4) != 4) {
            fprintf(2, "parent write error\n");
            exit(1); 
        }
        close(f2c[1]);
        wait((int*)0);
        if (read(c2f[0], parentbuf, MAXBUFSIZE) < 0) {
            fprintf(2, "parent read error\n");
            exit(1);
        }
        printf("%d: received %s\n",getpid(),parentbuf);
        close(c2f[0]);
    }
    exit(0);
}