#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// concurrent primes seive
void seive() {
    int a;
    read(0, (char*)&a, 4);
    printf("prime %d\n", a);
    int p[2];
    pipe(p);
    if (fork() == 0) { // child
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
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
    }
}

int main() {
    int p[2];
    if (fork() == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        seive(); 
    } else {
        close(p[0]);
        for(int i = 2; i < 36; ++i) {
            write(p[1], (char*)&i, 4);
        }
        close(p[1]);
    } 
}