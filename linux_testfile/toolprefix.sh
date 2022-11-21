#! /usr/bin/env bash
if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1 
then 
    echo 'riscv64-unknown-elf-'; 
elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; 
then 
    echo 'riscv64-linux-gnu-'; 
elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; 
then 
    echo 'riscv64-unknown-linux-gnu-'; 
else 
    echo "***" 1>&2; 
    echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; 
    echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; 
    echo "***" 1>&2; exit 1; 
fi
# riscv64-unknown-elf- 

if cat test.txt
then 
    echo "cat test.txt"
else
    echo "failed"
fi