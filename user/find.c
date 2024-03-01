#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(2, "ERROR: find <dir> <file name>\n");
        exit(-1);
    }
    
    exit(0);
}