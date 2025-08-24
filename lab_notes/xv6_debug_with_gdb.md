# Debugging with GDB
- https://pdos.csail.mit.edu/6.828/2021/labs/guidance.html
- 
For kernel or user-space debugging:

1. Start xv6 in gdb server mode:
In one terminal:
```bash
cd xv6-riscv
make qemu-gdb 
# This runs QEMU and pauses at startup, listening on port 26000 for GDB.

# *** Now run 'gdb' in another window.
# qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -S -gdb tcp::26000

```

2. Load the debug symbols for your program 
In a separate terminal:

```bash
# riscv64-linux-gnu-gdb / riscv64-unknown-elf-gdb
gdb-multiarch
# user/_primes is a RISC-V ELF
# loaded the user/_primes binary ,just loads debug symbols into GDB
# it hasn’t actually started xv6 or loaded your program into the emulator’s memory yet.
(gdb) file user/_find

# if we want to dubug kernel code
gdb-multiarch kernel/kernel

# enable tui window
(gdb) tui enable
# show the asm code
(gdb) layout asm
(gdb) layout reg
(gdb) focus reg
(gdb) focus asm
(gdb) layout src # show the source code in window
# split window to show both asm and source code
(gdb) layout split


# This attaches GDB to QEMU
(gdb) target remote :26000 
# Remote debugging using :26000
# 0x0000000000001000 in ?? ()

# set a break point, followed by followed by 'c' (continue), and xv6 will run until it hits the breakpoint.
(gdb) b main
# Breakpoint 1 at 0x1ec: file user/primes.c, line 79.
(gdb) c
# Continuing.

```

3. Run _primes inside xv6

In the QEMU console (still running in the make qemu-gdb terminal), type:
```bash

$ make qemu-gdb
# *** Now run 'gdb' in another window.
# qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -S -gdb tcp::26000

# xv6 kernel is booting

# hart 2 starting
# hart 1 starting
# prinit: starting sh

$ primes
# When _primes starts, GDB will hit your breakpoint
```


3. Set a breakpoint:
 
```bash
Thread 3 hit Breakpoint 1, main (argc=1, argv=0x2fe0) at user/primes.c:79
79      {
```


## kernel/kernel.asm

If you want to see what the assembly is that the compiler generates for the kernel or to find out what the instruction is at a particular kernel address, see the file kernel.asm, which the Makefile produces when it compiles the kernel. (The Makefile also produces .asm for all user programs.)


If the kernel panics, it will print an error message listing the value of the program counter when it crashed; you can search kernel.asm to find out in which function the program counter was when it crashed, or you can run addr2line -e kernel/kernel pc-value (run man addr2line for details). If you want to get backtrace, restart using gdb: run 'make qemu-gdb' in one window, run gdb (or riscv64-linux-gnu-gdb) in another window, set breakpoint in panic ('b panic'), followed by followed by 'c' (continue). When the kernel hits the break point, type 'bt' to get a backtrace.

If your kernel hangs (e.g., due to a deadlock) or cannot execute further (e.g., due to a page fault when executing a kernel instruction), you can use gdb to find out where it is hanging. Run run 'make qemu-gdb' in one window, run gdb (riscv64-linux-gnu-gdb) in another window, followed by followed by 'c' (continue). When the kernel appears to hang hit Ctrl-C in the qemu-gdb window and type 'bt' to get a backtrace.

## info mem
qemu has a "monitor" that lets you query the state of the emulated machine. You can get at it by typing control-a c (the "c" is for console). A particularly useful monitor command is info mem to print the page table. You may need to use the cpu command to select which core info mem looks at, or you could start qemu with make CPUS=1 qemu to cause there to be just one core.
