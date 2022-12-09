#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
//char ch[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
const char* str[] = { "00000002", "00000003", "00000004", "00000005", "00000006", "00000007", "00000008",
                      "00000009", "0000000a", "0000000b", "0000000c", "0000000d", "0000000e", "0000000f", 
                      "00000010", "00000011", "00000012", "00000013", "00000014", "00000015", "00000016", 
                      "00000017", "00000018", "00000019", "0000001a", "0000001b", "0000001c", "0000001d",
                      "0000001e", "0000001f", "00000020", "00000021", "00000022", "00000023", 0};
// void int2str(int a, char buf[]) {

// }
int trans(const char* str) {
   if (strlen(str) != 8) {
        printf("str is %s\n", str);
        printf("len is %d\n", strlen(str));
        exit(1);
   } 
   const char* p = str + strlen(str);
   p --;
   int ans = 0;
   int i = 1;
   while (p>=str) {
        char c = *p;
        if ('0' <= c && c <= '9') {
            ans += (c-'0')*i;
        }
        else if (c >= 'a' && c <= 'f') {
            ans += ((c - 'a')+10)*i;
        }
        else {
            fprintf(2, "Not hex bit\n");
            exit(1);
        }
        i *= 16;
        p--;
   }
   if (str[0] >= 'a') {
        ans = ans - i;
   }
   return ans;
}

void seive() {
    //int base = 0;
    char buf[10] = {0};
    int n = read(0, buf, 8);
    if (n == 0)
        exit(0);
    int base = trans(buf);
    printf("prime %d\n", base);
   
    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(0);      
        dup(p[0]);     
        close(p[0]);      
        close(p[1]);  
        seive(); 
        exit(0);
    }
    else {
        close(p[0]);
        while (read(0, buf, 8) == 8) {
            int data;
            data = trans(buf);
            if (data%base) {
                write(p[1], buf, 8);
            }
        }
        close(p[1]);
        wait((int*)0);
    }

   return;
}
int main() {
    int p[2];
    pipe(p);
  
    if (fork() == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        seive();
        exit(0);
    }
    else {
        close(p[0]);
        for (int i = 0; i < 34; ++i) {
            write(p[1], str[i], 8);
        }
        close(p[1]);
        wait((int*)0);
    }
    exit(0);
}