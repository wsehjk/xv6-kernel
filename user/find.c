#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"

// find all the files in a directory tree with a specific name.
const char* fileName(const char* path) {
    //static char buf[512];
    const char* p;
    for (p = path + strlen(path); p >= path && *p != '/'; --p)
    ;
    p++;
    return p;
} 
void find(const char* path, const char* pattern) {
    struct stat st;
    int fd;
    char buf[512];
    char *p;
    if ((fd = open(path,O_RDONLY)) < 0) {
        fprintf(2, "cannot open file %s\n", path);
        close(fd);
        return;
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "cannot fstat file %s\n", path);
        close(fd);
        return;
    }
    if (st.type == T_FILE || st.type == T_DEVICE) {
        if (strcmp(fileName(path), pattern) == 0) {
            printf("%s\n", path);
        }
    }
    else if (st.type == T_DIR) { // recursion, donot recure into . and ..
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            fprintf(2, "find: path too long\n");
            return;
        }
        strcpy(buf, path);
        p = buf + strlen(path);
        *p++ = '/';
        struct dirent de;
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            //printf("file is %s\nde.inum is %d\n", de.name, de.name);
            if (de.name[0] == '.')  // donnot recurse into . and .. 
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0) {
               printf("cannot stat file %s\n", buf);
               continue;
            }
            find(buf, pattern);
        }
    }
    close(fd);
    return;
}
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: %s <directory> <pattern>\n", argv[0]);
        exit(0);
    }
    // 
    find(argv[1], argv[2]);
    exit(0);
}