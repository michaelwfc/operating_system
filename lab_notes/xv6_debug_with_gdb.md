# Debugging with GDB

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
gdb-multiarch
# user/_primes is a RISC-V ELF
# loaded the user/_primes binary ,just loads debug symbols into GDB
# it hasn’t actually started xv6 or loaded your program into the emulator’s memory yet.

(gdb) file user/_find
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
