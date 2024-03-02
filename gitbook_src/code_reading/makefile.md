# xv6编译
配好环境之后，在每个实验分支下，执行`make qemu`就可以启动`xv6 shell`。
```makefile
qemu: $K/kernel fs.img
	$(QEMU) $(QEMUOPTS)
```

<img src=image.png div align="center" width="50%" height="50%">

`qemu`目标依赖`kernel/kernel`，`fs.img`；
## kernel/kernel
`kernel/kernel`就是内核代码，由内核的`.o`文件，`.ld`文件，以及`usr/initcode`。注意到`usr/initcode`没有链接到`kernel/kernel`里。
```makefile
$K/kernel: $(OBJS) $K/kernel.ld $U/initcode
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS) 
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$U/initcode: $U/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm
```

`kernel/*.o`文件由同名的`.c`文件通过隐式规则推导得到。如
```bash
riscv64-unknown-elf-gcc -Wall -Werror -O -fno-omit-frame-pointer -ggdb -DSOL_UTIL -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib -mno-relax -I. -fno-stack-protector -fno-pie -no-pie   -c -o kernel/fs.o kernel/fs.c
```

## fs.img

`fs.img`是存储用户程序的文件，由`mkfs/mkfs`生成, `mkfs/mkfs`由`mkfs/mkfs.c`编译得到。可以看到`mkfs/mkfs`只用来制作`fs.img`，不会作为`xv6`读取的用户程序或者内核程序。
```makefile
fs.img: mkfs/mkfs README $(UEXTRA) $(UPROGS)
	mkfs/mkfs fs.img README $(UEXTRA) $(UPROGS)
```
`UPROGS`是`user/_*`可执行文件，由同名的`.o`文件和四个库文件`$U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o`链接而成，
`.o`文件和`$U/ulib.o $U/printf.o $U/umalloc.o`由隐式规则推导得到
`$U/usys.o`由`$U/usys.pl`文件得到，系统调用的指令
```
传递参数
设置系统调用号
ecall指令
```
```makefile
ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$U/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/usys.S
$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S
```