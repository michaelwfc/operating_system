
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

## Target
Your first job is to modify the kernel so that every process uses its own copy of the kernel page table when executing in the kernel. 

- Modify `struct proc` to maintain a kernel page table for each process
- modify the `scheduler` to switch kernel page tables when switching processes. 
For this step, each per-process kernel page table should be identical to the existing global kernel page table. 

Read the book chapter and code mentioned at the start of this assignment; it will be easier to modify the virtual memory code correctly with an understanding of how it works. Bugs in page table setup can cause traps due to missing mappings, can cause loads and stores to affect unexpected pages of physical memory, and can cause execution of instructions from incorrect pages of memory.

## Some hints:
- Add a field to struct proc for the process's kernel page table.
- A reasonable way to produce a kernel page table for a new process is to implement a modified version of kvminit that makes a new page table instead of modifying kernel_pagetable. You'll want to call this function from allocproc.
- Make sure that each process's kernel page table has a mapping for that process's kernel stack. In unmodified xv6, all the kernel stacks are set up in procinit. You will need to move some or all of this functionality to allocproc.
- Modify scheduler() to load the process's kernel page table into the core's satp register (see kvminithart for inspiration). Don't forget to call sfence_vma() after calling w_satp().
- scheduler() should use kernel_pagetable when no process is running.
- Free a process's kernel page table in freeproc.
- You'll need a way to free a page table without also freeing the leaf physical memory pages.
- vmprint may come in handy to debug page tables.
- It's OK to modify xv6 functions or add new functions; you'll probably need to do this in at least kernel/vm.c and kernel/proc.c. (But, don't modify kernel/vmcopyin.c, kernel/stats.c, user/usertests.c, and user/stats.c.)
- A missing page table mapping will likely cause the kernel to encounter a page fault. It will print an error that includes sepc=0x00000000XXXXXXXX. You can find out where the fault occurred by searching for XXXXXXXX in kernel/kernel.asm.

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





## Solution

### 1. Adding a kernel_pagetable field to `struct proc`
- Add a field to `struct proc` for the process's kernel page table.
- A reasonable way to produce a kernel page table for a new process is to implement a modified version of `kvminit` that makes a new page table instead of modifying `kernel_pagetable`. You'll want to call this function from `allocproc` .



### 2. Creates kernel page table and maps kernel stack for each new process

- Make sure that each process's kernel page table has a mapping for that process's kernel stack. 
  In unmodified xv6, all the kernel stacks are set up in `procinit`
  You will need to move some or all of this functionality to `allocproc`.

In vanilla xv6:
- There’s one single global kernel page table for all processes.
- That page table has mappings for every process’s kernel stack.
So any process can run in kernel mode, and its kernel stack VA → PA translation is valid.

1. kernel stacks are stetup in `procinit`
   ```c
   uint64 va = KSTACK((int) (p - proc));
   kvmmap(kernel_pagetable, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
   ```
2. set as stack pointer in `usertrapret`
   ```c
   p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
   ```
3. The CPU switches to process A’s kernel stack in `uservec`:
   ```asm
   csrw sscratch, a0        # Save user a0
   ld sp, 8(a0)             # Load kernel stack pointer
   ```

Now we’ll move kernel stack mapping into each process’s kpagetable at `allocproc()` (where you allocate the process structure)


### 3. Make the scheduler switch kernel page tables

- Modify `scheduler()`  in kernel/proc.c to load the process's kernel page table into the core's `satp register` 
  (see `kvminithart` for inspiration). 
  Don't forget to call `sfence_vma()` after calling `w_satp()`.
  `scheduler()` should use kernel_pagetable when no process is running.

Two things must happen when the CPU switches which process is “current”:
- 1. Between processes (in the `scheduler`, while in the kernel): we must load the chosen process’s kpagetable into `satp` and flush the `TLB`.
- 2. On traps from user to kernel (`uservec` in trampoline.S): the trampoline switches to the kernel pagetable by loading the global variable kernel_pagetable. Therefore, we must keep that global variable pointing at the current process’s kpagetable before we let the process run.


#### Reasoning

In original xv6, there is only one global kernel page table (kernel_pagetable).
So, all processes share that one `satp` value whenever the CPU is executing in kernel mode.

```asm
ld t1, 0(a0)       # load p->trapframe->kernel_satp
csrw satp, t1
sfence.vma zero, zero
```

When switching between processes: 
The scheduler() doesn’t touch satp at all — because the kernel is always running under the same global kernel page table.

What changes in the assignment:
In this assignment, you’re asked to make each process have its own kernel page table.
That means:
- Each process p now has p->kpagetable
- When p is running in the kernel (e.g. during a syscall or trap), the CPU must use that process’s own kernel page table
- And this p->kpagetable is different for each process (it may map different kernel stacks, or even later, user memory).

Otherwise, you’d still be using the previous process’s kernel mappings while trying to run a different process — and that’s unsafe.

And when the CPU goes idle (no process running), it should revert to:
```c
if(!found){
      // CPU idle path — no process is runnable
      // Use global kernel page table
      w_satp(MAKE_SATP(kernel_pagetable));
      sfence_vma();
    }
```
✅ When CPU is idle (no process running): use global kernel page table
✅ When switching to a process: load that process’s kernel page table

so that the kernel itself can still run.

So both are needed:
- uservec(in trampoline.S) ensures the same process switches to its kernel page table during trap.
- scheduler ensures that different processes use their own kernel page tables.


### 4. Free a process's kernel page table

- Free a process's kernel page table in `freeproc`.
  You'll need a way to free a page table without also freeing the leaf physical memory pages.


Every process now owns an extra page of memory (or several pages) that hold its kernel page table structure (the page table pages themselves).

When a process exits (e.g., exit() → freeproc()), if we don’t free these pages:
- The kernel’s memory allocator (kalloc()) will never reclaim them.
- Over time, as many processes are created and destroyed, the kernel will run out of free memory — a memory leak.

So, we must free each process’s kernel page table when the process is destroyed.


A RISC-V Sv39 page table is a 3-level tree:
```
root page table (level 2)
  ├─ entries pointing to next-level page tables (level 1)
  │    ├─ entries pointing to final-level (level 0)
  │    │    ├─ entries pointing to *leaf physical memory pages*
```
Leaf physical pages are the actual memory pages that store data/code, e.g.:
- user program memory
- kernel text/data
- kernel stacks
- device memory

Each leaf entry (PTE) has the PTE_V bit set and points directly to physical memory.
```c
pte_t pte = walk(pagetable, va, 0);
if(pte & PTE_V && (pte & (PTE_R | PTE_W | PTE_X)))
  // This is a leaf mapping

```


So, when we free the page table, we only want to free the page table pages themselves, not the leaf physical pages they point to — because:
- kernel memory pages (like text, data, trampoline) are shared by all processes,
- and user memory pages are freed separately in freeproc() via uvmfree().

How to free a process’s kernel page table safely
xv6 already provides a helper:
```c
void freewalk(pagetable_t pagetable);
```
Defined in kernel/vm.c.
It recursively frees all page table pages (except leaf mappings).

However — the vanilla freewalk() also frees the leaf pages for user page tables, because it’s designed to clean up after uvmfree().

For kernel page tables, that’s too aggressive — those leaf pages (like the kernel text, trampoline, etc.) are shared, not owned by the process.

So, you must write a modified version, e.g.:

```c
void
freewalk_noleaf(pagetable_t pagetable)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){
      uint64 child = PTE2PA(pte);
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // this PTE points to a lower-level page table
        freewalk_noleaf((pagetable_t)child);
      }
      // else, it's a leaf PTE — skip freeing its physical memory
      pagetable[i] = 0;
    }
  }
  kfree((void*)pagetable);
}

// Then in freeproc():
if(p->kpagetable){
  freewalk_noleaf(p->kpagetable);
  p->kpagetable = 0;
}


```



### others
- `vmprint` may come in handy to debug page tables.
- It's OK to modify xv6 functions or add new functions; you'll probably need to do this in at least kernel/vm.c and kernel/proc.c. (But, don't modify kernel/vmcopyin.c, kernel/stats.c, user/usertests.c, and user/stats.c.)
- A missing page table mapping will likely cause the kernel to encounter a page fault. It will print an error that includes sepc=0x00000000XXXXXXXX. You can find out where the fault occurred by searching for XXXXXXXX in kernel/kernel.asm.









## Test
You pass this part of the lab if `usertests` runs correctly.
Read the book chapter and code mentioned at the start of this assignment; it will be easier to modify the virtual memory code correctly with an understanding of how it works. Bugs in page table setup can cause traps due to missing mappings, can cause loads and stores to affect unexpected pages of physical memory, and can cause execution of instructions from incorrect pages of memory.

If usertests passes, you can be confident that:

- Your per-process kernel page tables are correctly created
- The scheduler correctly switches between page tables
- System calls continue to work as expected
- Memory management operations function properly
- All existing kernel functionality is preserved

```bash
# In the xv6_labs_2021 directory
make qemu

# Then in the QEMU terminal
$ usertests
# This will run all tests in the usertests suite. If your implementation is correct, all tests should pass. The usertests suite is designed to thoroughly test the system call interface and various kernel functionalities that would be affected by page table changes.


# Running specific tests (optional)
# In QEMU terminal
$ usertests forktest
$ usertests sbrk
$ usertests mem


For the "A kernel page table per process" assignment, simply running usertests and seeing "ALL TESTS PASSED" is sufficient to validate that your implementation is correct. The test suite is comprehensive enough to catch any issues with your page table implementation because it exercises all the kernel functionality that would be affected by this change.



```


## Debug: echo hi
<!-- write(fd, buf, n) -->

User program passes buf = 0x4000 (user VA).

Kernel runs sys_write().

Kernel can’t just do *buf because its page table doesn’t map 0x4000.

Instead, it calls `copyin(p->pagetable, kbuf, buf, n)` which:
- Uses `walk(p->pagetable, buf)` to find the physical address.
- Copies data into a kernel buffer (kbuf).

Then the kernel writes kbuf to the device.





### Debug write
```bash
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

```


# 3. Simplify copyin/copyinstr (hard)

## Target
The kernel's copyin function reads memory pointed to by user pointers. It does this by translating them to physical addresses, which the kernel can directly dereference. It performs this translation by walking the process page-table in software. 

Your job in this part of the lab is to add user mappings to each process's kernel page table (created in the previous section) that allow `copyin` (and the related string function copyinstr) to directly dereference user pointers.

Replace the body of `copyin` in kernel/vm.c with a call to `copyin_new` (defined in kernel/vmcopyin.c); do the same for `copyinstr` and `copyinstr_new`. Add mappings for user addresses to each process's kernel page table so that copyin_new and copyinstr_new work. 



This scheme relies on the user virtual address range not overlapping the range of virtual addresses that the kernel uses for its own instructions and data. 

Xv6 uses virtual addresses that start at zero for user address spaces, and luckily the kernel's memory starts at higher addresses. However, this scheme does limit the maximum size of a user process to be less than the kernel's lowest virtual address. After the kernel has booted, that address is `0xC000000` in xv6, the address of the `PLIC` registers; 
see `kvminit()` in kernel/vm.c, kernel/memlayout.h, and Figure 3-4 in the text. You'll need to modify xv6 to prevent user processes from growing larger than the PLIC address.

## Some hints:

- Replace `copyin()` with a call to `copyin_new` first, and make it work, before moving on to `copyinstr`.
- At each point where the kernel changes a process's user mappings, change the process's kernel page table in the same way. Such points include fork(), exec(), and sbrk().
- Don't forget that to include the first process's user page table in its kernel page table in userinit.
- What permissions do the PTEs for user addresses need in a process's kernel page table? (A page with PTE_U set cannot be accessed in kernel mode.)
- Don't forget about the above-mentioned PLIC limit.


Linux uses a technique similar to what you have implemented. Until a few years ago many kernels used the same per-process page table in both user and kernel space, with mappings for both user and kernel addresses, to avoid having to switch page tables when switching between user and kernel space. However, that setup allowed side-channel attacks such as Meltdown and Spectre.

Explain why the third test srcva + len < srcva is necessary in copyin_new(): give values for srcva and len for which the first two test fail (i.e., they will not cause to return -1) but for which the third one is true (resulting in returning -1).

## Test

You pass this assignment if `usertests` runs correctly and all the make grade tests pass.


-------------------

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



