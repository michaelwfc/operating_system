Virtual memory provides a level of **indirection**:
the kernel can intercept memory references by marking `PTEs` invalid or read-only, leading to page faults, and can change what addresses mean by modifying PTEs. 
There is a saying in computer systems that any systems problem can be solved with **a level of indirection**. 
The lazy allocation lab provided one example. This lab explores another example: **copy-on write fork**.

To start the lab, switch to the cow branch:
```bash
$ git fetch
$ git checkout cow
$ make clean
```


# The problem

The fork() system call in xv6 copies all of the parent process's user-space memory into the child. 
If the parent is large, copying can take a long time. Worse, the work is often largely wasted; for example, a fork() followed by exec() in the child will cause the child to discard the copied memory, probably without ever using most of it. 
On the other hand, if both parent and child use a page, and one or both writes it, a copy is truly needed.


# The solution

The goal of copy-on-write (COW) fork() is to defer allocating and copying physical memory pages for the child until the copies are actually needed, if ever.
COW fork() creates just a pagetable for the child, with PTEs for user memory pointing to the parent's physical pages. COW fork() marks all the user PTEs in both parent and child as **not writable**. 

When either process tries to write one of these COW pages, the CPU will force a page fault. The kernel page-fault handler detects this case, allocates a page of physical memory for the faulting process, copies the original page into the new page, and modifies the relevant PTE in the faulting process to refer to the new page, this time with the PTE marked **writeable**. When the page fault handler returns, the user process will be able to write its copy of the page.

COW fork() makes freeing of the physical pages that implement user memory a little trickier. 
A given physical page may be referred to by multiple processes' page tables, and should be freed only when the last reference disappears.


# Implement copy-on write(hard)

Your task is to implement copy-on-write fork in the xv6 kernel. You are done if your modified kernel executes both the `cowtest` and `usertests` programs successfully.

# Test
To help you test your implementation, we've provided an xv6 program called `cowtest` (source in user/cowtest.c). cowtest runs various tests, but even the first will fail on unmodified xv6. Thus, initially, you will see:

```bash
$ cowtest
simple: fork() failed
$ 
```
The "simple" test allocates more than half of available physical memory, and then fork()s. The fork fails because there is not enough free physical memory to give the child a complete copy of the parent's memory.
When you are done, your kernel should pass all the tests in both cowtest and usertests. That is:

```bash
$ cowtest
simple: ok
simple: ok
three: zombie!
ok
three: zombie!
ok
three: zombie!
ok
file: ok
ALL COW TESTS PASSED
$ usertests
...
ALL TESTS PASSED
$
```

# Plan
Here's a reasonable plan of attack.

1. Modify `uvmcopy()` to map the parent's physical pages into the child, instead of allocating new pages. Clear `PTE_W` in the PTEs of both child and parent.

2. Modify `usertrap()` to recognize page faults. When a page-fault occurs on a COW page, allocate a new page with `kalloc()`, copy the old page to the new page, and install the new page in the PTE with `PTE_W` set.

3. Ensure that each physical page is freed when the last PTE reference to it goes away -- but not before. A good way to do this is to keep, for each physical page, a **"reference count"** of the number of user page tables that refer to that page. 
   - Set a page's reference count to one when `kalloc()` allocates it. 
   - Increment a page's reference count when fork causes a child to share the page, 
   - and decrement a page's count each time any process drops the page from its page table. 
   - `kfree()` should only place a page back on the free list if its reference count is zero. 
  It's OK to to keep these counts in a fixed-size array of integers. You'll have to work out a scheme for how to index the array and how to choose its size. For example, you could index the array with the page's physical address divided by 4096, and give the array a number of elements equal to highest physical address of any page placed on the free list by `kinit()` in kalloc.c.

4. Modify `copyout()` to use the same scheme as page faults when it encounters a COW page.

# Some hints:

- The lazy page allocation lab has likely made you familiar with much of the xv6 kernel code that's relevant for copy-on-write. However, you should not base this lab on your lazy allocation solution; instead, please start with a fresh copy of xv6 as directed above.
- It may be useful to have a way to record, for each PTE, whether it is a COW mapping. You can use the `RSW` (reserved for software) bits in the RISC-V PTE for this.
- `usertests` explores scenarios that `cowtest` does not test, so don't forget to check that all tests pass for both.
- Some helpful macros and definitions for page table flags are at the end of kernel/riscv.h.
- If a COW page fault occurs and there's no free memory, the process should be killed.

# Debug
## Debug 1
```bash
xv6 kernel is booting 
hart 2 starting 
hart 1 starting 
init: starting sh 
$ cowtest simple: ok 
simple: ok 
three: scause 0x000000000000000d sepc=0x0000000080000f0c stval=0x000000000000270f 
panic: kerneltrap


```

### kinit
```bash
b kinit
# void
# freerange(void *pa_start, void *pa_end)
# {
#   char *p;
#   p = (char*)PGROUNDUP((uint64)pa_start);
#   for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
#     kfree(p);
# }

(gdb) p/x pa_start
$12 = 0x80686000

# end is defined as a variable/symbol (like extern char end[];)
# Printing end gives its address/value
(gdb) p/x pa_end
$7 = 0x88000000

(gdb) set $KERNBASE=0x80000000L
(gdb) p/x $KERNBASE
$9 = 0x80000000
(gdb) p/x $KERNBASE +  128*1024*1024
$10 = 0x88000000

# Check symbol table
# Address 0x80000000 contains the _entry code
(gdb) info symbol 0x80000000
_entry in section .text

# _entry is code, shows instruction
# This is the INSTRUCTION at _entry, not the address!


(gdb) p/x _entry
$13 = 0x17 
# _entry is a function/code label, and when you print it directly, GDB shows the first instruction at that location, not the address.
# Printing _entry tries to dereference it (reads the instruction bytes)
# Need &_entry to get its address

# Always use & when getting the address of functions or labels:
(gdb) p/x &_entry
$14 = 0x80000000

# Use info address to get the address of a symbol
(gdb) info address _entry
Symbol "_entry" is at 0x80000000 in a file compiled without debugging.

(gdb) x/10i $KERNBASE
   0x80000000 <_entry>: auipc   sp,0x9
   0x80000004 <_entry+4>:       ld      sp,-1136(sp)
   0x80000008 <_entry+8>:       lui     a0,0x1
   0x8000000a <_entry+10>:      csrr    a1,mhartid
   0x8000000e <_entry+14>:      addi    a1,a1,1
   0x80000010 <_entry+16>:      mul     a0,a0,a1
   0x80000014 <_entry+20>:      add     sp,sp,a0
   0x80000016 <_entry+22>:      jal     ra,0x80000086 <start>
   0x8000001a <spin>:   j       0x8000001a <spin>
   0x8000001c <timerinit>:      addi    sp,sp,-16

(gdb) where
#0  kfree_inner (pa=pa@entry=0x80686000) at kernel/kalloc.c:113
#1  0x0000000080000e1a in page_ref_dec_debug (pa=pa@entry=2154323968,
    why=why@entry=0x80008008 <__FUNCTION__.1535> "kfree") at kernel/kalloc.c:165
#2  0x0000000080000e60 in kfree (pa=pa@entry=0x80686000) at kernel/kalloc.c:214
#3  0x0000000080000e9c in freerange (pa_start=<optimized out>, pa_end=pa_end@entry=0x88000000)
    at kernel/kalloc.c:203
#4  0x0000000080000efe in kinit () at kernel/kalloc.c:194
#5  0x0000000080001366 in main () at kernel/main.c:19

# r points to first free page
(gdb) p/x r
$3 = 0x80686000
# Garbage! (0x01 bytes from memset)
(gdb) p/x r->next
$4 = 0x101010101010101
# Why 0x101010101010101?
# memset(pa, 1, PGSIZE) fills the entire page with byte 0x01
# r->next is a pointer (8 bytes on 64-bit RISC-V)
# Reading 8 bytes of 0x01: 0x01 01 01 01 01 01 01 01 = 0x0101010101010101


# freelist is empty (NULL)
(gdb) p kmem.freelist 
$5 = (struct run *) 0x0


(gdb) p kmem.freelist.next
$6 = (struct run *) 0x0

# r->next = kmem.freelist;   // r->next = NULL (freelist is empty)
(gdb) p/x r->next
$7 = 0x0

# kmem.freelist = r;         // freelist now points to r
(gdb) p/x kmem.freelist
$9 = 0x80686000
(gdb) p kmem.freelist
$10 = (struct run *) 0x80686000

## After First Call - State:
kmem.freelist --> [0x80686000]
                      |
                      next = NULL


Second Call: kfree_inner(0x80687000)
After second call:
kmem.freelist --> [0x80687000] --> [0x80686000] --> NULL
## The Pattern:
Each new page is added to the **front** of the linked list:
After 1st free:  freelist → [page1] → NULL
After 2nd free:  freelist → [page2] → [page1] → NULL
After 3rd free:  freelist → [page3] → [page2] → [page1] → NULL

```


```
Memory Layout in xv6:
Physical Memory Layout:

0x80000000  KERNBASE
    |
    |---- Kernel code (.text)
    |---- Kernel data (.data, .bss)
    |---- End of kernel binary
    |
0x80686000  <-- pa_start (end)
    |
    |---- FREE MEMORY (heap for kalloc)
    |---- This is what gets added to free list
    |
0x88000000  <-- pa_end (PHYSTOP)

Why start at 0x80686000?

The symbol end marks the end of the kernel's binary. Everything from KERNBASE to end contains:

Kernel code (instructions)
Initialized data (.data section)
Uninitialized data (.bss section)
Kernel stack


Summary:

0x80000000 to 0x80686000: Kernel binary (code + data) - CANNOT BE FREED
0x80686000 to 0x88000000: Free memory (about 122 MB) - CAN BE ALLOCATED

If xv6 started freeing from KERNBASE (0x80000000), it would overwrite the running kernel code, causing an immediate crash!

```

## kvminit
```bash
b kvminit
(gdb) where
#0  kvminit () at kernel/vm.c:26
#1  0x0000000080001350 in main () at kernel/main.c:20
(gdb) p/x kernel_pagetable
$3 = 0x87fff000
```

## Debug 2
```bash
xv6 kernel is booting
kvminit kernel_pagetable: 0x0000000087fff000
hart 1 starting
hart 2 starting
DEC pa=0x0x0000000087f72000 idx=32626 -> ref=0 (kfree)
DEC pa=0x0x0000000087f70000 idx=32624 -> ref=0 (kfree)
DEC pa=0x0x0000000087f71000 idx=32625 -> ref=0 (kfree)
DEC pa=0x0x0000000087f73000 idx=32627 -> ref=0 (kfree)
DEC pa=0x0x0000000087f74000 idx=32628 -> ref=0 (kfree)
DEC pa=0x0x0000000087f75000 idx=32629 -> ref=0 (kfree)
DEC pa=0x0x0000000087f6f000 idx=32623 -> ref=0 (kfree)
init: starting sh
DEC pa=0x0x0000000087f6b000 idx=32619 -> ref=1 (kfree)
DEC pa=0x0x0000000087f68000 idx=32616 -> ref=1 (kfree)
DEC pa=0x0x0000000087f67000 idx=32615 -> ref=1 (kfree)
DEC pa=0x0x0000000087f70000 idx=32624 -> ref=0 (kfree)
DEC pa=0x0x0000000087f71000 idx=32625 -> ref=0 (kfree)
DEC pa=0x0x0000000087f73000 idx=32627 -> ref=0 (kfree)
DEC pa=0x0x0000000087f74000 idx=32628 -> ref=0 (kfree)
DEC pa=0x0x0000000087f75000 idx=32629 -> ref=0 (kfree)
DEC pa=0x0x0000000087f72000 idx=32626 -> ref=0 (kfree)
$ cowtest
COW page-fault handler: copy contents from old page to new page, mem=0x0000000087f5d000,pa=0x0000000087f5e000
DEC pa=0x0x0000000087f5e000 idx=32606 -> ref=1 (usertrap)
scause 0x000000000000000c
sepc=0x0000000000000b42 stval=0x0000000000000b42
panic: kerneltrap

hwo to trace the bug in cow lab
```


### usertrap
```bash
xv6 kernel is booting

kvminit kernel_pagetable: 0x0000000087fff000
hart 1 starting
hart 2 starting
init: starting sh
$ cowtest

b usertrap
```

## Debug 3

```bash
$ make clean && make CPUS=1 qemu

xv6 kernel is booting

kvminit kernel_pagetable: 0x0000000087fff000
hart 1 starting
hart 2 starting
init: starting sh
$ cowtest
simple: ok
simple: ok
three: ok
three: ok
three: ok
file: eererrorror::rr ror r: eeaard df aeifaaleidd
le fdai
led
$

```

This shows concurrent output corruption - multiple processes are writing to the console at the same time, causing garbled output like "eererrorror::rr ror". This is a race condition in your COW implementation.
The Problem:
The file test in cowtest creates multiple processes that access shared file descriptors. Your COW implementation has a race condition in the reference counting of physical pages.
Debugging Steps:
Step 1: Run with single CPU to isolate the issue
bash# Edit Makefile, find CPUS line and change to:
CPUS := 1
Or run with:
make CPUS=1 qemu
If it works with 1 CPU but fails with multiple CPUs, it's definitely a race condition.


```bash
xv6 kernel is booting

kvminit kernel_pagetable: 0x0000000087fff000
init: starting sh
$ cowtest
simple: ok
simple: ok
three: ok
three: ok
three: ok
file: error: read failed
error: read failed
error: read failed
```


Good! With CPUS=1 the garbled output is gone, confirming it's a race condition. Now we see the actual error: "read failed".
This means the file test can't read from the file correctly after fork. The bug is likely in how you handle shared file descriptors and COW pages during reads.


### Debug 4
```bash
xv6 kernel is booting

kvminit kernel_pagetable: 0x0000000087fff000
init: starting sh
$ cowtest
simple: ok
simple: ok
three: ok
three: ok
three: ok
file: ok
ALL COW TESTS PASSED
$ usertests
usertests starting
test execout: cowfault error: pa2=0, usertrap scause=0x0x000000000000000f sepc=0x0x0000000000002ac0 stval=0x0x0000000000010b88
OK
test copyin: OK
test copyout: OK
test copyinstr1: OK
test copyinstr2: OK
test copyinstr3: OK
test rwsbrk: OK
test truncate1: OK
test truncate2: OK
test truncate3: OK
test reparent2: OK
test pgbug: OK
test sbrkbugs: usertrap(): unexpected scause 0x000000000000000c pid=3248
            sepc=0x000000000000555e stval=0x000000000000555e
diagnose: fault va 0x0x000000000000555e
  pte = 0x0x0000000000000000
  PTE_V:0 PTE_R:0 PTE_W:0 PTE_X:0 PTE_U:0 PTE_COW(software):0x0x0000000000000000
  PPN/PA = 0x0x0000000000000000
usertrap(): unexpected scause 0x000000000000000c pid=3249
            sepc=0x000000000000555e stval=0x000000000000555e
diagnose: fault va 0x0x000000000000555e
  pte = 0x0x0000000000000000
  PTE_V:0 PTE_R:0 PTE_W:0 PTE_X:0 PTE_U:0 PTE_COW(software):0x0x0000000000000000
  PPN/PA = 0x0x0000000000000000
OK
test badarg: OK
test reparent: OK
test twochildren: OK
test forkfork: OK
test forkforkfork: OK
test argptest: OK
test createdelete: OK
test linkunlink: OK
test linktest: OK
test unlinkread: OK
test concreate: OK
test subdir: OK
test fourfiles: OK
test sharedfd: OK
test dirtest: OK
test exectest: OK
test bigargtest: OK
test bigwrite: OK
test bsstest: OK
test sbrkbasic: OK
test sbrkmuch: OK
test kernmem: usertrap(): unexpected scause 0x000000000000000d pid=6229
            sepc=0x0000000000002026 stval=0x0000000080000000
diagnose: fault va 0x0x0000000080000000
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6230
            sepc=0x0000000000002026 stval=0x000000008000c350
diagnose: fault va 0x0x000000008000c350
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6231
            sepc=0x0000000000002026 stval=0x00000000800186a0
diagnose: fault va 0x0x00000000800186a0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6232
            sepc=0x0000000000002026 stval=0x00000000800249f0
diagnose: fault va 0x0x00000000800249f0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6233
            sepc=0x0000000000002026 stval=0x0000000080030d40
diagnose: fault va 0x0x0000000080030d40
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6234
            sepc=0x0000000000002026 stval=0x000000008003d090
diagnose: fault va 0x0x000000008003d090
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6235
            sepc=0x0000000000002026 stval=0x00000000800493e0
diagnose: fault va 0x0x00000000800493e0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6236
            sepc=0x0000000000002026 stval=0x0000000080055730
diagnose: fault va 0x0x0000000080055730
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6237
            sepc=0x0000000000002026 stval=0x0000000080061a80
diagnose: fault va 0x0x0000000080061a80
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6238
            sepc=0x0000000000002026 stval=0x000000008006ddd0
diagnose: fault va 0x0x000000008006ddd0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6239
            sepc=0x0000000000002026 stval=0x000000008007a120
diagnose: fault va 0x0x000000008007a120
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6240
            sepc=0x0000000000002026 stval=0x0000000080086470
diagnose: fault va 0x0x0000000080086470
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6241
            sepc=0x0000000000002026 stval=0x00000000800927c0
diagnose: fault va 0x0x00000000800927c0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6242
            sepc=0x0000000000002026 stval=0x000000008009eb10
diagnose: fault va 0x0x000000008009eb10
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6243
            sepc=0x0000000000002026 stval=0x00000000800aae60
diagnose: fault va 0x0x00000000800aae60
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6244
            sepc=0x0000000000002026 stval=0x00000000800b71b0
diagnose: fault va 0x0x00000000800b71b0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6245
            sepc=0x0000000000002026 stval=0x00000000800c3500
diagnose: fault va 0x0x00000000800c3500
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6246
            sepc=0x0000000000002026 stval=0x00000000800cf850
diagnose: fault va 0x0x00000000800cf850
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6247
            sepc=0x0000000000002026 stval=0x00000000800dbba0
diagnose: fault va 0x0x00000000800dbba0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6248
            sepc=0x0000000000002026 stval=0x00000000800e7ef0
diagnose: fault va 0x0x00000000800e7ef0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6249
            sepc=0x0000000000002026 stval=0x00000000800f4240
diagnose: fault va 0x0x00000000800f4240
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6250
            sepc=0x0000000000002026 stval=0x0000000080100590
diagnose: fault va 0x0x0000000080100590
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6251
            sepc=0x0000000000002026 stval=0x000000008010c8e0
diagnose: fault va 0x0x000000008010c8e0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6252
            sepc=0x0000000000002026 stval=0x0000000080118c30
diagnose: fault va 0x0x0000000080118c30
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6253
            sepc=0x0000000000002026 stval=0x0000000080124f80
diagnose: fault va 0x0x0000000080124f80
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6254
            sepc=0x0000000000002026 stval=0x00000000801312d0
diagnose: fault va 0x0x00000000801312d0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6255
            sepc=0x0000000000002026 stval=0x000000008013d620
diagnose: fault va 0x0x000000008013d620
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6256
            sepc=0x0000000000002026 stval=0x0000000080149970
diagnose: fault va 0x0x0000000080149970
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6257
            sepc=0x0000000000002026 stval=0x0000000080155cc0
diagnose: fault va 0x0x0000000080155cc0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6258
            sepc=0x0000000000002026 stval=0x0000000080162010
diagnose: fault va 0x0x0000000080162010
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6259
            sepc=0x0000000000002026 stval=0x000000008016e360
diagnose: fault va 0x0x000000008016e360
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6260
            sepc=0x0000000000002026 stval=0x000000008017a6b0
diagnose: fault va 0x0x000000008017a6b0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6261
            sepc=0x0000000000002026 stval=0x0000000080186a00
diagnose: fault va 0x0x0000000080186a00
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6262
            sepc=0x0000000000002026 stval=0x0000000080192d50
diagnose: fault va 0x0x0000000080192d50
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6263
            sepc=0x0000000000002026 stval=0x000000008019f0a0
diagnose: fault va 0x0x000000008019f0a0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6264
            sepc=0x0000000000002026 stval=0x00000000801ab3f0
diagnose: fault va 0x0x00000000801ab3f0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6265
            sepc=0x0000000000002026 stval=0x00000000801b7740
diagnose: fault va 0x0x00000000801b7740
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6266
            sepc=0x0000000000002026 stval=0x00000000801c3a90
diagnose: fault va 0x0x00000000801c3a90
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6267
            sepc=0x0000000000002026 stval=0x00000000801cfde0
diagnose: fault va 0x0x00000000801cfde0
  walk returned NULL pte
usertrap(): unexpected scause 0x000000000000000d pid=6268
            sepc=0x0000000000002026 stval=0x00000000801dc130
diagnose: fault va 0x0x00000000801dc130
  walk returned NULL pte
OK
test sbrkfail: usertrap(): unexpected scause 0x000000000000000d pid=6280
            sepc=0x00000000000040c6 stval=0x0000000000012000
diagnose: fault va 0x0x0000000000012000
  pte = 0x0x0000000000000000
  PTE_V:0 PTE_R:0 PTE_W:0 PTE_X:0 PTE_U:0 PTE_COW(software):0x0x0000000000000000
  PPN/PA = 0x0x0000000000000000
OK
test sbrkarg: OK
test validatetest: OK
test stacktest: usertrap(): unexpected scause 0x000000000000000d pid=6284
            sepc=0x0000000000002196 stval=0x000000000000fba0
diagnose: fault va 0x0x000000000000fba0
  pte = 0x0x0000000021f5710b
  PTE_V:1 PTE_R:1 PTE_W:0 PTE_X:1 PTE_U:0 PTE_COW(software):0x0x0000000000000001
  PPN/PA = 0x0x0000000087d5c000
OK
test opentest: OK
test writetest: OK
test writebig: OK
test createtest: OK
test openiput: OK
test exitiput: OK
test iput: OK
test mem: OK
test pipe1: OK
test preempt: kill... wait... OK
test exitwait: OK
test rmdot: OK
test fourteen: OK
test bigfile: OK
test dirfile: OK
test iref: OK
test forktest: OK
test bigdir: OK
ALL TESTS PASSED
```