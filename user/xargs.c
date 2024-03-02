#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[]) {
    char NEWLINE = '\n';
    char SPACE = ' ';
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
            // 将argbuf 按照space 解析
            argbuf[k] = '\0';
            int p = j;
            nextargv[p++] = argbuf;
            if (p == MAXARG + 1) {
                fprintf(2, "ERROR: too many args");
                exit(-1); 
            }
            for(int i = 0; i < k; i++) {
                if (argbuf[i] == SPACE) {
                    argbuf[i] = '\0';
                    nextargv[p++] = argbuf + (i + 1);
                    if (p == MAXARG + 1) {
                       fprintf(2, "ERROR: too many args");
                       exit(-1); 
                    }
                }
            }
            k = 0;
            nextargv[p] = 0;

            if (fork() == 0) {  // child
                exec(nextargv[0], nextargv);
            } else {
                wait(0);
            }
        } else {
            argbuf[k++] = c;
            if (k == MAXARGSIZE) {
                fprintf(2, "ERROR: argument too long\n");
                exit(-1);
            }
        }
    }
    exit(0);
}