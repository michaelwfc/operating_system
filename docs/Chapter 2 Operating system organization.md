


# Preparation: 
Read chapter 2 and xv6 code: kernel/proc.h, kernel/defs.h, kernel/entry.S, kernel/main.c, user/initcode.S, user/init.c, and skim kernel/proc.c and kernel/exec.c

# Reading

[13] David Patterson and Andrew Waterman. The RISC-V Reader: an open architecture Atlas.Strawberry Canyon, 2017.
[2] The RISC-V instruction set manual: user-level ISA. https://riscv.org/specifications/isa-spec-pdf/, 2019.
[1] The RISC-V instruction set manual: privileged architecture. https://riscv.org/specifications/privileged-isa/, 2019.



Thus an operating system must fulfill three requirements: 
- multiplexing 
- isolation
- interaction



# 2.1 Abstracting physical resources

Unix Interface:  abstracting physical resources
Processes:  abstraction of CPU
exec: abstraction of memory
files: abstraction of disk blocks

 
- Strong isolation 
Strong isolation requires a hard boundary between applications and the operating system.
To achieve strong isolation it’s helpful to forbid applications from directly accessing sensitive
hardware resources, and instead to **abstract the resources into services**.

- Time sharing
Unix transparently switches hardware CPUs among processes, saving and restoring register state as necessary, so that applications **don’t have to be aware of time sharing**.

# 2.2 User mode/Kernel mode


RISC-V has three modes in which the CPU can execute instructions: 
- machine mode 
  Instructions executing in machine mode have full privilege; a CPU starts in machine mode. Machine
mode is mostly intended for configuring a computer. Xv6 executes a few lines in machine mode and then changes to supervisor mode.

## Kernel mode/Supervisor mode 
In supervisor mode the CPU is allowed to execute privileged instructions:
  - enabling and disabling interrupts, 
  - reading and writing the register that holds the address of a page table

If an application in user mode attempts to execute **a privileged instruction**, then the CPU doesn’t execute the instruction, but switches to **supervisor mode** so that supervisor-mode code can terminate the application, because it did something it shouldn’t be doing


  - running in user space
  An application can execute only user-mode instructions (e.g., adding numbers, etc.) and is said to be running in user space, 
  - running in kernel space
    while the software in supervisor mode can  also execute privileged instructions and is said to be running in kernel space. 
  - kernel
  The software running in kernel space (or in supervisor mode) is called the kernel.

## ecall: How to invoke a kernel function
An application that wants to invoke a kernel function (e.g., the read system call in xv6) must transition to the kernel; an application **cannot** invoke a kernel function directly.


CPUs provide a special instruction that switches the CPU from user mode to supervisor mode and enters the kernel at an entry point specified by the kernel. 
(RISC-V provides the **ecall** instruction for this purpose.)

### ecall

`ecall` is a system call instruction that transfers control from user mode to kernel mode

What ecall does:
- ecall (Environment Call) is a RISC-V instruction that triggers a system call
- It causes a trap/exception that transfers control to the kernel's system call handler
- The processor switches from user mode to supervisor/machine mode
- Control jumps to the kernel's trap handler (usually in `stvec` register (trampoline page))
  uservec → saves registers, switches to kernel stack
  usertrap() → handles the trap in kernel
  sys_write() → actual syscall implementation
  usertrapret() → prepares return to user
  userret → restores registers, returns to user

It is important that the kernel control the entry point for transitions to supervisor mode;
##  user mode.

# 2.3 Kernel organization

A key design question is what part of the operating system should run in supervisor mode. 

## Monolithic kernel

One
possibility is that the entire operating system resides in the kernel, so that the implementations of
all system calls run in supervisor mode. This organization is called a monolithic kernel.

Operating systems: 
- Many Unix kernels are monolithic.
- Xv6

## Microkernel
![image](../images/Figure%202.1-A%20microkernel%20with%20a%20file-system%20server.png)

To reduce the risk of mistakes in the kernel, OS designers can minimize the amount of operating
system code that runs in supervisor mode, and execute the bulk of the operating system in user
mode. This kernel organization is called a microkernel

Operating systems
organized as a microkernel with servers
- Minix 
- L4
- QNX 


this microkernel design. In the figure, 
- the file system runs as a user-level process. 
- OS services running as processes are called servers. 
- To allow applications to interact with the file server, the kernel provides an inter-process communication mechanism to send messages
from one user-mode process to another. 
For example, if an application like the shell wants to read or write a file, it sends a message to the file server and waits for a response.

### user-level servers

In a microkernel, the kernel interface consists of a few low-level functions for starting applications,
sending messages, accessing device hardware, etc. This organization allows the kernel to be
relatively simple, as most of the operating system resides in user-level servers.


# 2.4 Code: xv6 organization

- The xv6 kernel source is in the kernel/sub-directory
- The inter-module interfaces are defined in `defs.h` (kernel/defs.h)
It contains function prototypes for every kernel function (from different .c files).
It works like a central declaration hub, so that different kernel source files can call each other’s functions without including each file separately.
It helps the compiler catch errors (wrong arguments, missing return values).


| File          | Description                                                  |
|---------------|--------------------------------------------------------------|
| bio.c         | Disk block cache for the file system.                        |
| console.c     | Connect to the user keyboard and screen.                     |
| entry.S       | Very first boot instructions.                                |
| exec.c        | exec() system call.                                          |
| file.c        | File descriptor support.                                     |
| fs.c          | File system.                                                 |
| kalloc.c      | Physical page allocator.                                     |
| kernelvec.S   | Handle traps from kernel, and timer interrupts.              |
| log.c         | File system logging and crash recovery.                      |
| main.c        | Control initialization of other modules during boot.         |
| pipe.c        | Pipes.                                                       |
| plic.c        | RISC-V interrupt controller.                                 |
| printf.c      | Formatted output to the console.                             |
| proc.c        | Processes and scheduling.                                    |
| sleeplock.c   | Locks that yield the CPU.                                    |
| spinlock.c    | Locks that don't yield the CPU.                              |
| start.c       | Early machine-mode boot code.                                |
| string.c      | C string and byte-array library.                             |
| swtch.S       | Thread switching.                                            |
| syscall.c     | Dispatch system calls to handling function.                  |
| sysfile.c     | File-related system calls.                                   |
| sysproc.c     | Process-related system calls.                                |
| trampoline.S  | Assembly code to switch between user and kernel.             |
| trap.c        | C code to handle and return from traps and interrupts.       |
| uart.c        | Serial-port console device driver.                           |
| virtio_disk.c | Disk device driver.                                          |
| vm.c          | Manage page tables and address spaces.                       |

Figure 2.2: Xv6 kernel source files.



# 2.5 Process overview

The mechanisms used by the kernel to implement processes include the user/supervisor mode flag, address spaces, and time-slicing of threads.

## Address Space
![image](../images/Figure%202.3-Layout%20of%20a%20process’s%20virtual%20address%20space.png)
To help enforce isolation, the process abstraction provides the illusion to a program that it has its own private machine.  
A process provides a program with what appears to be a **private memory system**, or **address space**, which other processes cannot read or write.

###  Page tables & Virtual Address
Xv6 uses **page tables** (which are implemented by hardware) to give each process its own address space.

The RISC-V **page table** translates (or “maps”) a **virtual address** (the address that an RISC-V instruction manipulates) to a** physical address** (an address that the CPU chip sends to main memory).


### Process’s address space
Xv6 maintains a separate page table for each process that defines that process’s address space.

an address space includes the process’s user memory starting at virtual address zero.

- Instructions come first, 
- followed by global variables, 
- then the stack
- finally a “heap” area (for malloc) that the process can expand as needed. 
  
  There are a number of factors that limit the maximum size of a process’s address space: pointers on the RISC-V are 64 bits
wide; the hardware only uses the low 39 bits when looking up virtual addresses in page tables; and xv6 only uses 38 of those 39 bits. Thus, the maximum address is 2^38 - 1 = 0x3fffffffff, which is MAXVA (kernel/riscv.h:363).

- At the top of the address space xv6 reserves a page for a trampoline and
- a page mapping the process’s trapframe.



The xv6 kernel maintains many pieces of state for each process, which it gathers into a struct proc (kernel/proc.h:86). 
A process’s most important pieces of kernel state are its page table, its kernel stack, and its run state. We’ll use the notation p->xxx to refer to elements of the proc structure;
for example, p->pagetable is a pointer to the process’s page table.

## Thread
Each process has a thread of execution (or thread for short) that executes the process’s instructions.
A thread can be suspended and later resumed. 
To switch transparently between processes, the kernel suspends the currently running thread and resumes another process’s thread. 
Much of the state of a thread (local variables, function call return addresses) is stored on the thread’s stacks.
Each process has two stacks: a user stack and a kernel stack (p->kstack).


# 2.6 Code: starting xv6


## Compiling xv6

Kernel:

       gcc          assembler          linker
proc.c ---> proc.s ---> proc.o
....                                   ld    ---> kernel
pip.c  ---> pipe.s ---> pipe.o


```bash
cd xv6-riscv

make CPUS=1 qemu-gdb


# in another terminal
gdb-multiarch

# you must tell it which program (ELF with symbols) to use:
# This loads the xv6 kernel ELF produced by the build system (kernel/kernel, sometimes kernel/kernel.asm is the disassembly, not the ELF).

(gdb) file kernel/kernel

# After that, you connect to QEMU’s gdb stub:
(gdb) target remote :26000

# set breakpoint at _entry in kernel/kenel.asm: first instuction in qemu emulator
b _entry

# set breakpoint at main in kernel/main.c
b main

# set breakpoint at panic in kernel/proc.c
b userinit

# Use layout asm for an assembly window.
# show the asm window and source code window
layout split
```
## Starting xv6, The first process and system call

When the RISC-V computer powers on, it initializes itself and runs a boot loader which is stored in read-only memory. 
The boot loader loads the xv6 kernel into memory. 
Then, in machine mode, the CPU executes xv6 starting at _entry (kernel/entry.S:7). 
The RISC-V starts with paging hardware disabled: virtual addresses map directly to physical addresses.
The loader loads the xv6 kernel into memory at physical address 0x80000000. 
The reason it places the kernel at 0x80000000 rather than 0x0 is because the address range 0x0:0x80000000 contains I/O devices.

### _entry
The instructions at _entry set up a stack so that xv6 can run C code. 

Xv6 declares space for an initial stack, stack0, in the file start.c (kernel/start.c:11). 
The code at _entry loads the stack pointer register sp with the address stack0+4096, the top of the stack, because the stack on RISC-V grows down. 

Now that the kernel has a stack, _entry calls into C code at start (kernel/start.c:21).
The function start performs some configuration that is only allowed in machine mode, and then switches to supervisor mode. 

### enter supervisor mode
To enter supervisor mode, RISC-V provides the instruction mret. This instruction is most often used to return from a previous call from supervisor mode to machine mode. 
start isn’t returning from such a call, and instead sets things up as if there had been one: 
- it sets the previous privilege mode to supervisor in the register mstatus, 
- it sets the return address to main by writing main’s address into the register mepc, disables virtual address translation in supervisor mode by writing 0 into the page-table register satp, and delegates all interrupts and exceptions to supervisor mode.

Before jumping into supervisor mode, start performs one more task: 
it programs the clock chip to generate timer interrupts.With this housekeeping out of the way, start “returns” to supervisor mode by calling mret. 
This causes the program counter to change to main (kernel/main.c:11).

After main (kernel/main.c:11) initializes several devices and subsystems, 

### calling userinit
it creates the first process by calling userinit (kernel/proc.c:226). 
The first process executes a small program written in RISC-V assembly, make makes the first system call in xv6. 
1. initcode.S (user/initcode.S:3) loads the number for the exec system call, SYS_EXEC (kernel/syscall.h:8), into register a7, and then calls ecall to re-enter the kernel. 
2. The kernel uses the number in register a7 in syscall (kernel/syscall.c:133) to call the desired system call. 
The system call table (kernel/syscall.c:108) maps SYS_EXEC to sys_exec, which the kernel invokes. 
3. As we saw in Chapter 1, exec replaces the memory and registers of the current process with a new program (in this case, /init).
4. Once the kernel has completed exec, it returns to user space in the /init process. 
5. Init (user/init.c:15) creates a new console device file if needed and then opens it as file descriptors 0, 1, and 2. 
Then it starts a shell on the console. The system is up.


