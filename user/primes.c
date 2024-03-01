#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// concurrent primes seive
void seive() {  // 从标准输入中读取数字，向管道中写
    int a;
    if (read(0, (char*)&a, 4) == 0 || a ==0) {  // 注意结束
        return ;
    }
    printf("prime %d\n", a);
    int p[2];
    pipe(p);
    if (fork() == 0) { // child
        close(0);      // 关闭 stdin
        dup(p[0]);     // 重启 stdin
        close(p[0]);
        close(p[1]); // 关闭

        seive();
    } else {
        close(p[0]);
        int b;
        while(read(0, &b ,4) != 0) {
            if (b%a != 0) {
                write(p[1], (char*)&b, 4);
            }
        }
        close(p[1]);
        wait(0);
    }
}

int main() {
    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(0);   // 关闭 stdin
        dup(p[0]);  // 重启 stdin
        close(p[0]);
        close(p[1]);
        
        seive(); 
    } else {
        close(p[0]);
        for(int i = 2; i < 36; ++i) {
            write(p[1], (char*)&i, 4);
        }
        close(p[1]);
        wait(0);
    } 
    exit(0);
}