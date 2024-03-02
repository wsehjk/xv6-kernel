# coding

## util lab
`util`实验借助已有的系统调用实现几个有趣的`user program`，借此加强对`pipe`，`fork`，`exec`等调用的理解
#### primes
```c
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
```
#### find

`find`要借助`open`获取文件描述符，使用`fstat`获得文件类型，如果是`T_FILE`比较文件名即可，如果是`T_DIR`则需要读取目录中的`direntry`获取文件名递归查询，
