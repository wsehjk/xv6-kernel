#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[]) {
    char NEWLINE = '\n';
    int MAXARGSIZE = 75;
    if (argc < 2) {
        printf("ERROR: xargs cmd arguments...\n");
        exit(-1);
    }
    char* nextargv[MAXARG+1];
    int j = 0;
    for(int i = 1; argv[i]; ++i) {
        nextargv[j++] = argv[i];
    }
    char c;
    char argbuf[MAXARGSIZE];  // 一个参数的最大长度 设置为75
    int k = 0;
    while(read(0, &c, 1) != 0) {
        if (c == NEWLINE) {
            nextargv[j] = argbuf;
            nextargv[j+1] = 0;
            argbuf[k] = '\0';
            k = 0;
            if (fork() == 0) {  // child
                exec(nextargv[0], nextargv);
            } else {
                wait(0);
            }
        } else {
            argbuf[k++] = c;
        }
    }
    exit(0);
}