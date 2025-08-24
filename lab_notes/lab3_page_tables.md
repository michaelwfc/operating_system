
In this lab you will explore page tables and modify them to to speed up certain system calls and to detect which pages have been accessed.

Before you start coding, read Chapter 3 of the xv6 book, and related files:

- `kern/memlayout.h`, which captures the layout of memory.
- `kern/vm.c`, which contains most virtual memory (VM) code.
- `kernel/kalloc.c`, which contains code for allocating and freeing physical memory.
It may also help to consult the RISC-V privileged architecture manual.


# Speed up system calls (easy)
Some operating systems (e.g., Linux) speed up certain system calls by sharing data in a read-only region between userspace and the kernel. This eliminates the need for kernel crossings when performing these system calls. To help you learn how to insert mappings into a page table, your first task is to implement this optimization for the `getpid()` system call in xv6.

When each process is created, map one read-only page at `USYSCALL` (a VA defined in memlayout.h). At the start of this page, store a struct usyscall (also defined in memlayout.h), and initialize it to store the PID of the current process. For this lab, `ugetpid()` has been provided on the userspace side and will automatically use the USYSCALL mapping. You will receive full credit for this part of the lab if the ugetpid test case passes when running `pgtbltest`.

## Some hints:

- You can perform the mapping in `proc_pagetable()` in `kernel/proc.c`.
- Choose permission bits that allow userspace to only read the page.
- You may find that `mappages()` is a useful utility.
- Don't forget to allocate and initialize the page in `allocproc()`.
- Make sure to free the page in `freeproc()`.
  Which other xv6 system call(s) could be made faster using this shared page? Explain how.
