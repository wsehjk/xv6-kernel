#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    struct sysinfo info;
    if (sysinfo(&info) < 0) {
        printf("FAIL: sysinfo failed");
        exit(1);
    }
    printf("fremem: %d, procs: %d\n", info.freemem, info.nproc);
    exit(0);
}