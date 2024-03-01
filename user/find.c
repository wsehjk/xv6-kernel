#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char* getname(char* path) {
    char* p = path + strlen(path);
    for(; p >= 0 && *p != '/'; p--)
        ;
    return p + 1;
}
void find(char *path, char* name) {
    char buf[512];
    int fd;
    char* p;
    struct dirent de;
    struct stat st;
    char* CURDIR = ".";
    char* PARENTDIR = "..";
    if ((fd = open(path, O_RDONLY)) < 0) {  // 读文件
        fprintf(2, "ERROR: cannot open file %s\n", path);
        exit(-1);
    }

    if (fstat(fd, &st) < 0) {   // 读文件元信息
        fprintf(2, "ERROR: cannot stat file %s\n", path);
        close(fd);
        exit(-1);
    }
    switch (st.type)
    {
    case T_FILE:
        p = getname(path);
        if (strcmp(p, name) == 0) {
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf("ls: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf+strlen(buf);
        *(p++) = '/';
        while(read(fd, &de, sizeof(de))) {
            if (de.inum == 0) {
                continue;
            }
            if (strcmp(de.name, CURDIR) == 0 || strcmp(de.name, PARENTDIR) == 0) {
                continue;
            }
            memcpy(p, de.name, DIRSIZ);
            // *(p + DIRSIZ) = '\0';
            p[DIRSIZ] = '\0';
            find(buf, name);    // 递归
        }

    default:
        break;
    }

    close(fd);
}
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(2, "ERROR: find <dir> <file name>\n");
        exit(-1);
    }
    find(argv[1], argv[2]);
    exit(0);
}