#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("ERROR: sleep time\n");
        exit(-1);
    }
    int t = atoi(argv[1]);
    sleep(t);
    exit(0);
}