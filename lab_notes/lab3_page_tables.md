
In this lab you will explore page tables and modify them to to speed up certain system calls and to detect which pages have been accessed.

Before you start coding, read Chapter 3 of the xv6 book, and related files:

- `kern/memlayout.h`, which captures the layout of memory.
- `kern/vm.c`, which contains most virtual memory (VM) code.
- `kernel/kalloc.c`, which contains code for allocating and freeing physical memory.
It may also help to consult the RISC-V privileged architecture manual.





# 1. Print a page table (easy)
To help you learn about RISC-V page tables, and perhaps to aid future debugging, your first task is to write a function that prints the contents of a page table.

Define a function called `vmprint()`. It should take a `pagetable_t` argument, and print that pagetable in the format described below. 
Insert if(p->pid==1) vmprint(p->pagetable) in `exec.c` just before the return argc, to print the first process's page table. 

You receive full credit for this assignment if you pass the `pte printout test` of make grade.

Now when you start xv6 it should print output like this, describing the page table of the first process at the point when it has just finished exec()ing init:

```bash
xv6 kernel is booting

page table 0x0000000087f6e000
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000 flag 0x0000000000000001
.. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000 flag 0x0000000000000001
.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000 flag 0x000000000000001f  = 11111 ->UXWRV
.. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000 flag 0x000000000000000f  = 1111 ->  XWRV
.. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000 flag 0x000000000000001f
..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000 flag 0x0000000000000001
.. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000 flag 0x0000000000000001
.. .. ..510: pte 0x0000000021fdd807 pa 0x0000000087f76000 flag 0x0000000000000007 = 0111 ->  WRV
.. .. ..511: pte 0x000000002000280b pa 0x000000008000a000 flag 0x000000000000000b = 1011 -> X RV -> trampoline

```

- The first line displays the argument to vmprint. 
- After that there is a line for each PTE, including PTEs that refer to page-table pages deeper in the tree. 
- Each PTE line is indented by a number of " .." that indicates its depth in the tree. 
- Each PTE line shows the PTE index in its page-table page, the pte bits, and the physical address extracted from the PTE. Don't print PTEs that are not valid. 

In the above example, 
- the top-level page-table page has mappings for entries 0 and 255. 
- The next level down for entry 0 has only index 0 mapped, 
- and the bottom-level for that index 0 has entries 0, 1, and 2 mapped.

Your code might emit different physical addresses than those shown above. The number of entries and the virtual addresses should be the same.

## Some hints:

- You can put `vmprint()` in `kernel/vm.c`.
- Use the macros at the end of the file `kernel/riscv.h`.
- The function `freewalk` may be inspirational.
- Define the prototype for vmprint in `kernel/defs.h` so that you can call it from `exec.c`.
- Use %p in your printf calls to print out full 64-bit hex PTEs and addresses as shown in the example.
  
## Explain the output of vmprint in terms of Fig 3-4 from the text. 
What does page 0 contain? 
What is in page 2? 
When running in user mode, could the process read/write the memory mapped by page 1?


## Debuging vmprint

```bash
# start qemu
make clean && make qemu-gdb

#start gdb
$ gdb-multiarch -x .gdbinit_kernel kernel/kernel

(gdb) b vmprint
Breakpoint 1 at 0x8000223c: file kernel/vm.c, line 356.
(gdb) c
Continuing.

Breakpoint 1, vmprint (pagetable=0x87f6e000) at kernel/vm.c:356
356       printf("page table %p\n", pagetable);
=> 0x000000008000223c <vmprint+12>:     83 35 84 fe     ld      a1,-24(s0)
   0x0000000080002240 <vmprint+16>:     17 95 00 00     auipc   a0,0x9
   0x0000000080002244 <vmprint+20>:     13 05 05 f2     addi    a0,a0,-224 # 0x8000b160
   0x0000000080002248 <vmprint+24>:     97 e0 ff ff     auipc   ra,0xffffe
   0x000000008000224c <vmprint+28>:     e7 80 80 7b     jalr    1976(ra) # 0x80000a00 <printf>

(gdb) p pagetable
$1 = (pagetable_t) 0x87f6e000
(gdb) next
# qemu
page table 0x0000000087f6e000

# pte_t pte = pagetable[i]; 
(gdb) p pte
$3 = 0x21fda801

# get the flag of pte
(gdb) p/t pte&0x3ff
$6 = 1

(gdb) p/x (pte>>10)<<12
$9 = 0x87f6a000

(gdb) p/x child
$10 = 0x87f6a000

# printf("%d: pte %p pa %p\n", i, pte, child); 
# qemu
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000

# level-2
(gdb) p level
$13 = 2
(gdb) p pagetable
$15 = (pagetable_t) 0x87f6a000
(gdb) p/x pte
$16 = 0x21fda401
(gdb) p/x child
$17 = 0x87f69000

.. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000


#  level-3
(gdb) p level
$22 = 3
(gdb) p/x pte
$23 = 0x21fdac1f
(gdb) p pagetable
$24 = (pagetable_t) 0x87f69000
(gdb) p/x child
$25 = 0x87f6b000
.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000

```


## Test
```bash
# print 
make qemu

# test in terminal
./grade-lab-pgtbl pte printout
```



# 2. A kernel page table per process (hard)

- Xv6 has a single kernel page table that's used whenever it executes in the kernel. 
The kernel page table is a `direct mapping` to physical addresses, so that kernel virtual address x maps to physical address x. 
- Xv6 also has a separate page table for each process's user address space
  containing only mappings for that process's user memory, starting at virtual address zero. Because the kernel page table doesn't contain these mappings, user addresses are not valid in the kernel. 
Thus, when the kernel needs to use a user pointer passed in a system call (e.g., the buffer pointer passed to write()), the kernel must first translate the pointer to a physical address. 

The goal of this section and the next is to allow the kernel to **directly dereference user pointers**.

### What “directly dereference user pointers” means?

After the modification (per-process kernel page tables)
- Each process will have its own kernel page table (p->kpagetable).
- This kernel page table:
  - Contains all the normal kernel mappings (device memory, trampoline, etc.).
  - Also contains that process’s user memory mappings.

Now, when the process traps into the kernel:
- The CPU switches to that process’s kernel page table.
- That page table maps both kernel addresses and the user addresses of the process.
So the kernel can now do:
```C
char c = *user_ptr;   // works! user_ptr is mapped in this process's kernel page table
```

“Directly dereference user pointers” means that once you add user memory mappings into the kernel’s page table, the kernel can just treat user pointers (buf, addr) as normal memory addresses and read/write them directly, instead of calling `copyin()/copyout()` to translate them.

### Why is this useful?
- It simplifies kernel code.
- Eliminates the need for copyin()/copyout() in many places.
- But… it’s dangerous!
The kernel must validate pointers before using them (to avoid kernel crash or security bugs).
This is why in modern OS design, Linux and others are very careful about user ↔ kernel pointer access.




## Target
Your first job is to modify the kernel so that every process uses its own copy of the kernel page table when executing in the kernel. 

- Modify `struct proc` to maintain a kernel page table for each process
- modify the `scheduler` to switch kernel page tables when switching processes. 
For this step, each per-process kernel page table should be identical to the existing global kernel page table. 


## Some hints:

- Add a field to `struct proc` for the process's kernel page table.
- A reasonable way to produce a kernel page table for a new process is to implement a modified version of `kvminit` that makes a new page table instead of modifying `kernel_pagetable`. You'll want to call this function from `allocproc` .
- Make sure that each process's kernel page table has a mapping for that process's kernel stack. In unmodified xv6, all the kernel stacks are set up in `procinit`. You will need to move some or all of this functionality to `allocproc` .
- Modify `scheduler()` to load the process's kernel page table into the core's `satp register` (see `kvminithart` for inspiration). Don't forget to call `sfence_vma()` after calling `w_satp()`.
  `scheduler()` should use kernel_pagetable when no process is running.
- Free a process's kernel page table in `freeproc`.
  You'll need a way to free a page table without also freeing the leaf physical memory pages.
- vmprint may come in handy to debug page tables.
- It's OK to modify xv6 functions or add new functions; you'll probably need to do this in at least kernel/vm.c and kernel/proc.c. (But, don't modify kernel/vmcopyin.c, kernel/stats.c, user/usertests.c, and user/stats.c.)
- A missing page table mapping will likely cause the kernel to encounter a page fault. It will print an error that includes sepc=0x00000000XXXXXXXX. You can find out where the fault occurred by searching for XXXXXXXX in kernel/kernel.asm.

## Solution

1. Build a new kernel page table as `kvminit()` does
   
  b. Add a new field to `struct proc` to store the kernel page table


2. Creates kernel page table and maps kernel stack for each new process 
   `allocproc`

In stock xv6, `procinit()` maps all kernel stacks into the global kernel pagetable. 

Now we’ll move that mapping into each process’s kpagetable at `allocproc()` time.

   
1.  Make the scheduler switch kernel page tables
Two things must happen when the CPU switches which process is “current”:
a. Between processes (in the scheduler, while in the kernel): we must load the chosen process’s kpagetable into `satp` and flush the `TLB`.
b. On traps from user to kernel (uservec in trampoline.S): the trampoline switches to the kernel pagetable by loading the global variable kernel_pagetable. Therefore, we must keep that global variable pointing at the current process’s kpagetable before we let the process run.


6) Move/trim the old procinit() kernel-stack mapping
7) Sanity checks / gotchas







## Test
You pass this part of the lab if `usertests` runs correctly.
Read the book chapter and code mentioned at the start of this assignment; it will be easier to modify the virtual memory code correctly with an understanding of how it works. Bugs in page table setup can cause traps due to missing mappings, can cause loads and stores to affect unexpected pages of physical memory, and can cause execution of instructions from incorrect pages of memory.



# Background

## Kernel vs User address space
Two separate address spaces in xv6

1. Kernel address space
- Uses **one global kernel page table**
- Kernel virtual address = physical address (direct mapping).
- Contains code/data/stack for the kernel, device memory, etc.
- Does not contain user memory mappings.

2. User address space
- Each process has its own user page table.
- Contains the process’s code, data, stack, heap.
- Starts at virtual address 0 and grows upward.
- Does not contain kernel mappings.

So:
- User space cannot directly see or touch kernel memory.
- Kernel space cannot directly dereference user pointers.


## What’s a *kernel crossing*?

When a user program makes a system call (`read`, `write`, `gettimeofday`, etc.), it needs the kernel’s help. But the kernel and user processes run in **different CPU privilege levels**:
user processes cannot directly access kernel data/hardware — they must switch to privileged mode.

* **User mode**: restricted; can’t touch hardware or kernel memory.
* **Kernel mode**: full privileges.

A **kernel crossing** means switching from:

* user mode → kernel mode (entering the kernel)
* then back from kernel mode → user mode (returning to the program).

This is not just a function call. It involves:

1. **Trap/interrupt** instruction (`ecall` on RISC-V, `syscall` on x86-64).

   * CPU saves registers, switches page tables (if needed), jumps to the kernel’s trap handler.
2. **Kernel code executes** the system call implementation.
3. **Return to user mode** with `sret`/`sysret`, restoring registers and resuming user code.

---


##  Kernel crossings process
When a process makes a system call:
1. CPU Trap into the kernel
- CPU switches to kernel mode
- kernel page table is loaded, The kernel installs  kernel page table into `satp`.


2. Kernel code executes
- The kernel gets arguments from registers / user stack (e.g., pointer to a buffer).

So, the kernel must translate:
- The pointer is a user virtual address, but the kernel cannot just dereference it — because its page table doesn’t have that mapping.
- These walk the user process’s page table to find the physical address corresponding to the user pointer.
- Then the kernel can read/write the memory on behalf of the user.


So, if a syscall gives a pointer 0x4000 (user VA), the kernel cannot just do:
```C
char c = *user_ptr;   // illegal! kernel page table doesn't know 0x4000
```

Instead it must call `copyin()/copyout()`, which:
- 1. Looks up the process’s user page table (p->pagetable) to find the physical address.
- 2. Copies the bytes into kernel space.

So the kernel **cannot directly dereference a user pointer**; it has to go through extra copying code.

3. Return to user mode

When leaving the kernel (sret), the CPU switches back to the process’s pure user page table (p->pagetable), which contains only user mappings.



## Why is this costly?

Compared to a normal function call:

* **mode switch overhead**: CPU must change mode (user → kernel → user).
* **Context saving/restoring**: registers, stack pointer, program counter need to be preserved.
* **Pipeline flushes & TLB effects**: trap handling disrupts CPU pipelines and caching.
* **Security checks**: kernel must validate arguments (e.g., pointers passed from user).

So even for something simple like `gettimeofday()`, the overhead is **hundreds of cycles**, much slower than a plain function call.

---

## Why share data in a read-only page?

For some syscalls, the kernel doesn’t really need to *do work* every time:
For frequently used, read-only data (time, CPU info, syscall numbers), the kernel shares it in a special memory page, eliminating the need for repeated kernel crossings.

* Example: `gettimeofday()`, `clock_gettime()`.

  * The kernel maintains the time anyway (via timer interrupts).
  * Instead of requiring a kernel crossing every time, the kernel can expose a **read-only memory page** that contains the current time (or parameters to compute it).
  * User programs can then just read from that page — no trap into kernel needed.

This is called the **vsyscall** or **vDSO** mechanism in Linux.



# Example: write(fd, buf, n)

User program passes buf = 0x4000 (user VA).

Kernel runs sys_write().

Kernel can’t just do *buf because its page table doesn’t map 0x4000.

Instead, it calls `copyin(p->pagetable, kbuf, buf, n)` which:
- Uses `walk(p->pagetable, buf)` to find the physical address.
- Copies data into a kernel buffer (kbuf).

Then the kernel writes kbuf to the device.





## .gdbinit_my setting
```bash
set confirm off
set architecture riscv:rv64
symbol-file kernel/kernel
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes
set disassemble-next-line on
set print pretty on 
set print array on
add-symbol-file user/_sh
b sys_write
c

# But this will trigger for every write call (including shell prompt).
# b write 
```

## Debug

```bash
# in qemu terminal
make clean && make qemu-gdb
*** Now run 'gdb' in another window.
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 1 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -S -gdb tcp::26000


# in gdb terminal
# Kernel GDB (for system calls and kernel code)
# gdb-multiarch  kernel/kernel

gdb-multiarch user/_sh          # Debug the shell binary, not kernel

(gdb) add-symbol-file user/_sh
(gdb) b getcmd                  # Break at shell's getcmd function
(gdb) info b
(gdb) c
Continuing.

Breakpoint 1, write () at user/usys.S:41
41       ecall
(gdb) 

# after qemu ternimal 
xv6 kernel is booting

page table 0x0000000087f6e000
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
.. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
.. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
.. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
.. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
.. .. ..510: pte 0x0000000021fdd807 pa 0x0000000087f76000
.. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
init: starting sh


# qemu ternimal 
$ echo hi

# in gdb
Breakpoint 1, getcmd (buf=buf@entry=0x1520 <buf> "echo hi\n", nbuf=nbuf@entry=100) at user/sh.c:135
135     {

(gdb) p/x buf
$1 = 0x1520

(gdb) x/s buf
0x1520 <buf.1141>:      "echo hi\n"

(gdb) x/8c buf
0x1520 <buf.1141>:      101 'e' 99 'c'  104 'h' 111 'o' 32 ' '  104 'h' 105 'i'10 '\n'

(gdb) x/6i $pc
=> 0x0 <getcmd>:        addi    sp,sp,-32
   0x2 <getcmd+2>:      sd      ra,24(sp)
   0x4 <getcmd+4>:      sd      s0,16(sp)
   0x6 <getcmd+6>:      sd      s1,8(sp)
   0x8 <getcmd+8>:      sd      s2,0(sp)
   0xa <getcmd+10>:     addi    s0,sp,32

(gdb) where
#0  getcmd (buf=buf@entry=0x1520 <buf> "echo hi\n", nbuf=nbuf@entry=100)
    at user/sh.c:135
#1  0x0000000000000adc in main () at user/sh.c:159
#2  0x00000000000000de in runcmd (cmd=<optimized out>) at user/sh.c:68
Backtrace stopped: previous frame inner to this frame (corrupt stack?)


(gdb) info frame
# Stack level 0, frame at 0x3fa0:
#  pc = 0x0 in getcmd (user/sh.c:135); saved pc = 0xadc
#  called by frame at 0x3fe0
#  source language c.
#  Arglist at 0x3fa0, args: buf=buf@entry=0x1520 <buf> "echo hi\n", nbuf=nbuf@entry=100
#  Locals at 0x3fa0, Previous frame's sp is 0x3fa0
# Could not fetch register "ustatus"; remote failure reply 'E14'


# - a0 → first argument ,pointer to buffer → use x/s $a0 to see string
# - a1 → second argument,a1 = integer value (100) → use p $a1 to see value
(gdb) info reg a0 a1 a2 a7
a0             0x1520   5408
a1             0x64     100
a2             0x1      1
a7             0x3      3

# # Print a0 in hex
(gdb) p/x $a0
$7 = 0x1520   # Should match buf address
(gdb) p/x buf
$14 = 0x1520
(gdb) p/x &buf[0]
$15 = 0x1520

# Print a0 in decimal
(gdb) p $a0
$8 = 5408

# Show string at address in a0
(gdb) x/s $a0
0x1520 <buf.1141>:      "echo hi\n"

# # Second argument: nbuf (should be 100)
# a1 = integer value (100) → use p $a1 to see value
(gdb) p/x $a1
$10 = 0x64

(gdb) p $a1
$11 = 100

# Treats a1 as ADDRESS, reads memory at 0x64
(gdb) x/d $a1
0x64 <panic+16>:        -91  # Reading random memory at address 0x64!


 
# Signature of write: int write(int fd, const void *buf, int n);
# user/user.h:        int write(int, const void*, int);

# At this point, the arguments to write() are already in the standard RISC-V calling convention registers:
# - a0 → first argument
# - a1 → second argument,a1 is the pointer to the buffer in user space.
# - a2 → third argument
# - a7 → system call number (already loaded with SYS_write = 16)


(gdb) 
```