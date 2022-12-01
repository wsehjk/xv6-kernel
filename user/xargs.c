#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#define MAXBUFSIZE 75 
int main(int argc, char* argv[]) {
    if (argc == 1) {
        fprintf(2, "Usage: %s [-n[j] 1] <command> <arg...>\n", argv[0]);
        exit(1);
    }
    int i = 1;
    if (strcmp(argv[1], "-n") == 0) {
        i += 2;
    }
    int j = 0;
    int k = i;
    char* execarg[MAXARG+2];
    while (argv[i] != 0) { 
        execarg[j++] = argv[i++];
    }   // j - 1 arguments 
    char c; 
    char buf[MAXBUFSIZE];
    if (k == 1) {     // xargs command arg...
        int max = 33 - j; // max number of arguments to get 
        char argbuf[max][MAXBUFSIZE]; 
        int bi = 0;
        int argindex = 0;
        while (read(0, &c, 1) == 1) {
            if (c == '\n') {
                buf[bi++] = 0;
                bi = 0;
                strcpy(argbuf[argindex], buf);
                execarg[j++] = argbuf[argindex++];
                if (argindex == max) 
                    break;
            }else {
                buf[bi++] = c;
            }
        }
        execarg[j] = 0;
        if (fork() == 0) {   // child
            exec(argv[k], execarg);
        }
        else {
            wait((int*)0);
        }
    } else{ // -n 1 
       int bi = 0;
        while (read(0, &c, 1) == 1) {
            if (c == '\n') {
                buf[bi++] = 0;
                bi = 0;
                execarg[j] = buf;
                execarg[j+1] = 0;
                if (fork() == 0) {   // child
                    exec(argv[k], execarg);
                }
                else {
                    wait((int*)0);
                }
            }else {
                buf[bi++] = c;
            }
        } 
    }
    exit(0);
}