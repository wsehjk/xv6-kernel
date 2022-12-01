#include "kernel/types.h" 
#include "user/user.h"

#define MAXBUFSIZE 100
int main(int argc, char* argv[])
{
	char buf[MAXBUFSIZE];
    while(1) {
        int n = read(0, buf, sizeof(buf));
        if (n <= 0)
            break;
        if (write(1,buf, n) != n) {
            exit(1);
        }
    }
    exit(0);
}
