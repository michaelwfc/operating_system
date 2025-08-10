# Debugging with GDB

For kernel or user-space debugging:

1. Run with debug mode:
```bash
cd xv6-riscv
make qemu-gdb
```

2. In a separate terminal:

```bash
gdb or riscv64-linux-gnu-gdb
(gdb) file user/_yourprog  
```

3. Set a breakpoint:
 set a break point, followed by followed by 'c' (continue), and xv6 will run until it hits the breakpoint.
