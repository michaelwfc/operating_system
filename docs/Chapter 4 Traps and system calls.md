
## three kinds of event
There are three kinds of event which cause the CPU to set aside ordinary execution of instructions
and force a transfer of control to special code that handles the event. 

1. system call
One situation is a system call, when a user program executes the `ecall` instruction to ask the kernel to do something for it.

2. exception
Another situation is an exception: an instruction (user or kernel) does something illegal, such as divide by zero or use an invalid virtual address. 

3. interrupt
The third situation is a device interrupt, when a device signals that it needs attention, for example when the disk hardware finishes a read or write
request.

## The usual sequence of trap:

The usual sequence is that a trap forces a transfer of control into the kernel; 
- the kernel saves registers and other state so that execution can be resumed; 
- the kernel executes appropriate handler code (e.g., a system call implementation or device driver); 
- the kernel restores the saved state and returns from the trap; 
- the original code resumes where it left off.


## Xv6 trap handling proceeds in four stages: 
1. hardware actions taken by the RISC-V CPU, 
2. some assembly instructions that prepare the way for kernel C code, 
3. a C function that decides what to do with the trap, and
4. the system call or device-driver service routine. 
   
   
While commonality among the three trap types suggests that a kernel could handle all traps with a single code path, it turns out to be convenient to have separate code for three distinct cases: 

- traps from user space, 
- traps from kernel space
- timer interrupts. 

Kernel code (assembler or C) that processes a trap is often called a `handler`; the first handler instructions are usually written in assembler (rather than C) and are sometimes called a `vector`.


# 4.1 RISC-V trap machinery

Each RISC-V CPU has a set of `control registers` that the kernel writes to tell the CPU how to handle traps, and that the kernel can read to find out about a trap that has occured. 
The RISC-V documents contain the full story [1]. 
`riscv.h` (kernel/riscv.h:1) contains definitions that xv6 uses. Here’s an outline of the most important registers:

## Control and Status Registers to handle traps
###  `stvec` 
- stvec = Supervisor Trap Vector Base Address Register
- 指向了内核中处理trap的指令的起始地址
- The kernel writes the address of its trap handler here; the RISC-V jumps to the address in `stvec` to handle a trap.

### `sscratch`
- sscratch = Supervisor Scratch
- The kernel places a value here that comes in handy at the very start of a trap handler.

###  `sepc`
- sepc = Supervisor Exception Program Counter
- When a trap occurs, RISC-V saves the program counter here (since the pc is then overwritten with the value in stvec). 
- The `sret` (return from trap) instruction copies `sepc` to the pc. The kernel can write `sepc` to control where `sret` goes.

### `scause`
RISC-V puts a number here that describes the reason for the trap.



###  `sstatus` 
- The **SIE bit** in sstatus: Supervisor Interrupt Enable
  controls whether device interrupts are enabled. 
  If the kernel clears SIE, the RISC-V will defer device interrupts until the kernel sets SIE. 
- The **SPP bit**: Previous mode, 1=Supervisor, 0=User
  indicates whether a trap came from user mode or supervisor mode, and controls to what mode `sret` returns.

## Supervisor mode
- R/W Control registers
  The above registers relate to traps handled in `supervisor mode`, and they cannot be read or written in user mode. 
There is a similar set of control registers for traps handled in machine mode; 
xv6 uses them only for the special case of timer interrupts.
Each CPU on a multi-core chip has its own set of these registers, and more than one CPU may be handling a trap at any given time.

- use PTE w/o PTE_U

## RISC-V hardware trace when trap occurs

When it needs to force a trap, the RISC-V hardware does the following for all trap types (other than timer interrupts):
1. If the trap is a device interrupt, and the `sstatus` SIE bit is clear, don’t do any of the following.
2. Disable interrupts by clearing the SIE bit in `sstatus`.
3. Copy the `pc` to `sepc`.
4. Save the current mode (user or supervisor) in the SPP bit in `sstatus`.
5. Set `scause` to reflect the trap’s cause.
6. Set the mode to supervisor.
7. Copy `stvec` to the `pc`.
8. Start executing at the new `pc`.


# 4.2 Traps from user space

## High-level Picture
```
SH
    write()
      |
    ecall
----------------
    uservec     (kerneltrampoline.S)    userret
    usertrap()  (kernel/trap.c)         usertrapret
    syscall()
    sys_write()
```


Xv6 handles traps differently depending on whether it is executing in the kernel or in user code.
Here is the story for traps from user code; Section 4.5 describes traps from kernel code.

A trap may occur while executing in user space if the user program makes a system call (`ecall` instruction), or does something illegal, or if a device interrupts. 

The high-level path of a trap from user space is 
`uservec` (kernel/trampoline.S:16), 
then `usertrap` (kernel/trap.c:37); 
and when returning,`usertrapret` (kernel/trap.c:90) 
and then `userret` (kernel/trampoline.S:88).


[trap-note](https://mit-public-courses-cn-translatio.gitbook.io/mit6-s081/lec06-isolation-and-system-call-entry-exit-robert/6.1-trap)



## Switch page tables when trap from user space

A major constraint on the design of xv6’s trap handling is the fact that the RISC-V hardware does not switch page tables when it forces a trap. This means that the trap handler address in `stvec` must have a valid mapping in the user page table, since that’s the page table in force when the trap handling code starts executing. 

Furthermore, xv6’s trap handling code needs to switch to the kernel page table; in order to be able to continue executing after that switch, the kernel page table must also have a mapping for the handler pointed to by `stvec`.
Xv6 satisfies these requirements using a **trampoline page**. 

The trampoline page contains `uservec` (from kernel/trampoline.S uservec 汇编代码), the xv6 trap handling code that `stvec` points to. 
The trampoline page is mapped in every process’s page table at address `TRAMPOLINE`, which is at the end of the virtual address space so that it will be above memory that programs use for themselves. 
The trampoline page is also mapped at address TRAMPOLINE in the kernel page table. 

See Figure 2.3 and Figure 3.3. 
![image](../images/Figure%202.3-Layout%20of%20a%20process’s%20virtual%20address%20space.png)
![image](../images/Figure%203.3-%20xv6's%20kernel%20address%20space.png)

Because the trampoline page is mapped in the user page table, with the `PTE_U` flag, traps can start executing there in supervisor mode. 
Because the trampoline page is mapped at the same address in the kernel address space, the trap handler can continue to execute after it switches to the kernel page table.


## trampoline(跳板)
### trampoline page
#### What is the trampoline page?
- The trampoline page is a single page of assembly code (in kernel/trampoline.S) that is mapped at the same high virtual address in every process’s page table.
- When xv6 is compiled, this code is placed in the kernel’s physical memory (just like any other .text section). Let’s call that physical page address `trampoline_pa`.
- It contains the very first instructions that run when the CPU enters the kernel from user mode (`trap entry`), and the very last instructions that run when the kernel returns to user mode (`trap return`).
- The trampoline page exists on both sides (mapped in both user and kernel page tables).
- xv6 needs the trampoline for all processes, because that’s how every process transitions between user and kernel mode.
- Instead of making a separate copy for each process, xv6 just maps the same physical page (trampoline_pa) into every process’s page table.
  
  
Think of it as a bridge between:

- A process running in user space (with its own page table), and
- The kernel running in kernel space (with the kernel page table).

#### Why is it needed?

When a user program traps (syscall, interrupt, exception), the CPU:
1. Switches to `supervisor mode` (kernel mode).
2. Loads the program counter from the `stvec` register (the trap handler entry point).
3. But it **does not** automatically change the page table!, The `satp` register still points to the user page table, the current page table is still the user page table! 

What’s wrong here?
1. The user page table usually does not map kernel addresses.
2. That page table does not map kernel memory.
3. If `stvec` pointed to normal kernel code (say 0x80000000), the CPU would try to fetch that instruction using the user page table → page fault inside the trap entry itself → dead system.  the CPU couldn’t fetch the trap handler code.

The trampoline is the fix
- The trampoline page contains a tiny assembly routine (in trampoline.S) at a fixed virtual address (e.g., TRAMPOLINE = 0xffff_ffff_ffff_f000).
- xv6 maps the trampoline page into every process’s page table at the fixed high address.
  - Virtual address `TRAMPOLINE` → points to the trampoline code (shared kernel physical page).
    
  - Virtual address `TRAPFRAME` → points to the per-process trapframe (data area for saving registers)

- That way, when a trap happens, `stvec` points to this trampoline address, and the code is always accessible, regardless of which process’s page table is active.
- the CPU (still using the user’s page table!) can fetch instructions from the trampoline, because that page is guaranteed to exist in every user’s address space.

### when trap
when ecall instruction is executed
ecall 用 system call 的方式觸發 trap， 提高程序的权限，挑战程序的 trampoline
ecall is an exception instruction that triggers a trap into the kernel

In user space, it doesn’t execute normal code. Instead, the CPU switches to privileged mode, saves the current PC, and jumps to the kernel's trap handler.

From GDB’s perspective, the kernel code is not part of your user-space program (kernel/kernel ELF you loaded). It’s in a different memory space, usually not mapped in your debugging session.

當 trap 發生時，會做的事情：
- sstatus(Supervisor status rigister): 把現在的狀態 (user or supervisor) 紀錄在 sstatus 的 SPP bit
- scause: 把造成 trap 的原因紀錄在 scause
- pc -> sepc: 把 pc 複製到 sepc(Supervisor Exception Program Counter) 中，進入 supervisor mode 後，用來紀錄回到 user mode 時，要回到什麼 address 開始執行
- sscratch: 進入到 kernel space 前 sscratch 會儲存 trapframe 的位置, 用来存储 32 user registers
- stvec->pc: 當 trap 發生時，RISC-V 會把 stvec 放到 pc 中, stvec point to the beginning of the trampoline page
- pc: 開始根據 pc 往下執行 to the start of trampoline page, the very next instruction will be fetched from the trampoline page
- satp: Supervisor Address Translation and Protection Register, No change

1. save pc-> spec 将程序计数器 保存在 spec
   它几乎跟一个用户寄存器的地位是一样的，我们需要能够在用户程序运行中断的位置继续执行用户程序。

2. mode : user mode -> supervisor mode
   我们需要将mode改成 supervisor mode，因为我们想要使用内核中的各种各样的特权指令。

### trap entry

On trap entry (`uservec` in trampoline.S):
1. Save user registers into the process’s `trapframe`.
2. Switch `satp` from the user’s page table to the kernel page table.
3. Jump into the kernal trap handler (`usertrap()` in trap.c).

trap的最开始，CPU的所有状态都设置成运行用户代码而不是内核代码。在trap处理的过程中，我们实际上需要更改一些这里的状态，或者对状态做一些操作。这样我们才可以运行系统内核中普通的C程序。

kernel/trampoline.S uservec 汇编代码

对 ecall 瞬间的状态做快照
1. 填充 struct tramframe(proc.h),利用 sscratch register 保存所有寄存器到 tramframe
   我们需要保存32个用户寄存器。
   因为很显然我们需要恢复用户应用程序的执行，尤其是当用户程序随机的被设备中断所打断时。我们希望内核能够响应中断，之后在用户程序完全无感知的情况下再恢复用户代码的执行。所以这意味着32个用户寄存器不能被内核弄乱。但是这些寄存器又要被内核代码所使用，所以在trap之前，你必须先在某处保存这32个用户寄存器。


4. 切换到 内核栈（相当于切换到进程对应的内核线程）
   我们需要将堆栈寄存器指向位于内核的一个地址，因为我们需要一个堆栈来调用内核的C函数
6. 切换到内核的地址空间
   SATP寄存器现在正指向user page table，而user page table只包含了用户程序所需要的内存映射和一两个其他的映射，它并没有包含整个内核数据的内存映射。所以在运行内核代码之前，我们需要将SATP指向kernel page table
  - 修改 satp register, 将SATP指向kernel page table
  - sfence.vma
7. 跳转到 tramframe->kernel_trap
  痛苦时间解除，跳转到 c 代码
  一旦我们设置好了，并且所有的硬件状态都适合在内核中使用， 我们需要跳入内核的C代码。


### trap return

On trap return (`userret` in trampoline.S):
1. Switch `satp` back to the user’s page table.
2. Restore user registers from trapframe.
3. Execute `sret` to return to user mode.

Thus, the trampoline is the only safe place the CPU can land in, while still running under the user’s page table.

Why not just map the whole kernel in user page table?

- Security: If user processes could see all of kernel memory mappings, a bug/exploit could let user code read/write kernel memory.
- Simplicity: Only one page (the trampoline) is exposed in every user page table, not the entire kernel.


The trampoline page is essential because:
1. The CPU enters kernel mode before switching page tables.
2. The first instructions after a trap must be reachable in the user’s page table.
3. The trampoline provides this code — a tiny assembly bridge that switches to the kernel safely.
4. It ensures security by exposing only one shared page to user processes instead of mapping all of kernel memory.


## trampoline assembel code (kernel/trampoline.S)
### uservec
The code for the `uservec` trap handler is in trampoline.S (kernel/trampoline.S:16). 
When uservec starts, all 32 registers contain values owned by the interrupted user code. These 32 values need to be saved somewhere in memory, so that they can be restored when the trap returns to user space. Storing to memory requires use of a register to hold the address, but at this point there are no general-purpose registers available! 
Luckily RISC-V provides a helping hand in the form of the `sscratch` register. 

### csrrw instruction
The `csrrw` instruction at the start of `uservec` swaps the contents of `a0` and `sscratch`. 

sscratch points to where the process's p->trapframe
```asm
csrrw a0, sscratch, a0
```
Now the user code’s a0 is saved in sscratch; uservec has one register (a0) to play with; and a0 contains the value the kernel previously placed in sscratch.

### Save user registers to trapframe
uservec’s next task is to save the 32 user registers. 
Before entering user space, the kernel set `sscratch` to point to a per-process trapframe structure that (among other things) has space to save the 32 user registers (kernel/proc.h:44). 
Because `satp` still refers to the user page table, uservec needs the trapframe to be mapped in the user address space. 
When creating each process, xv6 allocates a page for the process’s trapframe, and arranges for it always to be mapped at user virtual address `TRAPFRAME`, which is just below `TRAMPOLINE`. 

The process’s p->trapframe also points to the trapframe, though at its physical address so the kernel can use it through the kernel page table.
Thus after swapping a0 and sscratch, a0 holds a pointer to the current process’s trapframe.
`uservec` now saves all user registers there, including the user’s a0, read from sscratch.

The `trapframe` contains 
- the address of the current process’s kernel stack
- the current CPU’s hartid
- the address of the usertrap function
- the address of the kernel page table. 
`uservec` retrieves these values, switches `satp` to the kernel page table, and calls `usertrap`.

## usertrap(kernel/trap.c)
The job of `usertrap` is to determine the cause of the trap, process it, and return (kernel/- trap.c:37). 
It first changes `stvec` so that a trap while in the kernel will be handled by kernelvec rather than uservec. It saves the `sepc` register (the saved user program counter), because usertrap might call yield to switch to another process’s kernel thread, and that process might return to user space, in the process of which it will modify sepc. If the trap is a system call, usertrap calls syscall to handle it; if a device interrupt, devintr; otherwise it’s an exception, and the kernel kills the faulting process. The system call path adds four to the saved user program counter because RISC-V, in the case of a system call, leaves the program pointer pointing to the ecall instruction but user code needs to resume executing at the subsequent instruction.

On the way out, usertrap checks if the process has been killed or should yield the CPU (if this trap is a timer interrupt).

## usertrapret(kernel/trap.c)
The first step in returning to user space is the call to `usertrapret` (kernel/trap.c:90). This function sets up the RISC-V control registers to prepare for a future trap from user space. This involves changing stvec to refer to uservec, preparing the trapframe fields that uservec relies
on, and setting sepc to the previously saved user program counter. At the end, usertrapret calls userret on the trampoline page that is mapped in both user and kernel page tables; the reason is that assembly code in userret will switch page tables.
usertrapret’s call to `userret` passes TRAPFRAME in a0 and a pointer to the process’s user page table in a1 (kernel/trampoline.S:88). 

## userret(kernel/trampoline.S)
`userret` switches satp to the process’s user page table. Recall that the user page table maps both the trampoline page and TRAPFRAME, but nothing
else from the kernel. The fact that the trampoline page is mapped at the same virtual address in user and kernel page tables is what allows uservec to keep executing after changing satp.
userret copies the trapframe’s saved user a0 to sscratch in preparation for a later swap with TRAPFRAME. From this point on, the only data userret can use is the register contents and the content of the trapframe. Next userret restores saved user registers from the trapframe, does
a final swap of a0 and sscratch to restore the user a0 and save TRAPFRAME for the next trap, and executes sret to return to user space.


## kernel stack


#### what is the kernel stack?
The kernel stack is a per-process memory region in **kernel space** used to store function calls, local variables, and saved registers when the CPU executes kernel code on behalf of that process. Each process has one, and user programs cannot access it.


Normal (user) stack vs. kernel stack
User stack:
- Lives in the process’s user address space.
- Holds function call frames, local variables, return addresses, etc. for user-mode code.
- Process can freely read/write it.
- Destroyed when the process exits.

Kernel stack:
- A separate stack allocated only for the kernel to use while it is running on behalf of that process.
- Lives in **kernel memory**, not in the process’s user address space.
- User code cannot touch it (no mapping in user page table).
- Used for system calls, traps, interrupts when the kernel executes code.


#### Why does each process need a kernel stack?

Imagine a process makes a system call:
1. CPU switches from user mode → kernel mode.
2. The kernel needs to run functions (sys_read(), sys_write(), etc.), push registers, handle traps.
3. The kernel cannot use the process’s user stack (unsafe, might not be mapped, or user could corrupt it).
4. Instead, it switches to the process’s kernel stack, which is private to the kernel and always valid.

So:
Each process has its own kernel stack, but kernel code (like sys_read) is shared across all processes.
That kernel stack stores the kernel’s call frames while running on behalf of that process.




# Trap Debug for system call

## Initial Debug write

```bash
# in qemu terminal
make clean && make qemu-gdb
QEMU 4.2.1 monitor - type 'help' for more information
(qemu) Gdk-Message: 23:11:40.757: Unable to load hand2 from the cursor theme


# in gdb terminal
gdb-multiarch  kernel/kernel
# === 加载 kernel/kernel 符号（确保能看到 trampoline/usertrap）===
(gdb) add-symbol-file user/_sh

# This attaches GDB to QEMU
(gdb) target remote :26000
Remote debugging using :26000
0x0000000000001000 in ?? ()

# get the address of write instruction from user/sh.asm
0000000000000dfa <write>:
.global write
write:
 li a7, SYS_write
     dfa:	48c1                	li	a7,16
 ecall
     dfc:	00000073          	ecall
 ret
     e00:	8082                	ret


(gdb) b *0xdfa 
Breakpoint 1 at 0xdfa: file user/usys.S, line 40.

(gdb) c
Continuing.

Breakpoint 1, write () at user/usys.S:40
40       li a7, SYS_write

(gdb) layout split
# (gdb) tui disabble

# the program counter (PC). At any point in time, the PC points at (contains the address of) some machine-language instruction in main memory
(gdb) where
#0  write () at user/usys.S:40
#1  0x0000000000000e94 in putc (fd=fd@entry=2, c=<optimized out>, c@entry=36 '$') at user/printf.c:12
#2  0x0000000000000fa0 in vprintf (fd=<optimized out>, fmt=fmt@entry=0x1380 "$ ", ap=ap@entry=0x3f50) at user/printf.c:64
#3  0x000000000000114a in fprintf (fd=fd@entry=2, fmt=fmt@entry=0x1380 "$ ") at user/printf.c:103
#4  0x0000000000000022 in getcmd (buf=buf@entry=0x1520 <buf> "", nbuf=nbuf@entry=100) at user/sh.c:136
#5  0x0000000000000adc in main () at user/sh.c:160
#6  0x00000000000000de in runcmd (cmd=<optimized out>) at user/sh.c:68
Backtrace stopped: previous frame inner to this frame (corrupt stack?)

(gdb) list
35       li a7, SYS_read
36       ecall
37       ret
38      .global write
39      write:
40       li a7, SYS_write
41       ecall
42       ret
43      .global close
44      close:


(gdb) p $pc
$1 = (void (*)()) 0xdfa <write>

# GDB by default only shows general-purpose registers
# On RISC-V, info reg lists:
# - integer registers (ra, sp, gp, tp, t0–t6, s0–s11, a0–a7)
# - pc
# But not CSRs (Control and Status Registers), like satp, sstatus, stvec, etc.

(gdb) info reg

ra             0xe94    0xe94 <putc+26>
sp             0x3e90   0x3e90
gp             0x505050505050505        0x505050505050505
tp             0x505050505050505        0x505050505050505
t0             0x505050505050505        361700864190383365
t1             0x505050505050505        361700864190383365
t2             0x505050505050505        361700864190383365
fp             0x3eb0   0x3eb0
s1             0x1381   4993
a0             0x2      2
a1             0x3e9f   16031
a2             0x1      1
a3             0x505050505050505        361700864190383365
a4             0x505050505050505        361700864190383365
a5             0x24     36
a6             0x505050505050505        361700864190383365
a7             0x15     21
s2             0x24     36
s3             0x0      0
s4             0x25     37
s5             0x2      2
s6             0x3f50   16208
s7             0x14c8   5320
s8             0x64     100
s9             0x6c     108
s10            0x78     120
s11            0x70     112
t3             0x505050505050505        361700864190383365
t4             0x505050505050505        361700864190383365
t5             0x505050505050505        361700864190383365
t6             0x505050505050505        361700864190383365
pc             0xdfa    0xdfa <write>


# the address of user space is quite small/low in xv6
# sp: stack pointer
sp             0x3e90   0x3e90
# pc: program counter
pc             0xdfa    0xdfa <write>



# examine memory at register a1 with 2 elements:each in character format(byte)
# “show 2 characters starting from the address stored in a1.”
(gdb) x/2c $a1
0x3e9f: 36 '$'  48 '0'
# a0: file descriptor to shell argument
# a1: the pointer to the buffer of chacters the shell want to write in a1
# a2: the number of characters to write in a2

(gdb) delete 1


# in the qemu console/monitor： ctrl-a c
# show the page table for the shell process
(qemu) info mem
vaddr            paddr            size             attr
---------------- ---------------- ---------------- -------
0000000000000000 0000000087f60000 0000000000001000 rwxu-a-  # first user page (code/data)
0000000000001000 0000000087f5d000 0000000000001000 rwxu-a-  # more code/data
0000000000002000 0000000087f5c000 0000000000001000 rwx----  # more code/data, 这个page是无效的，因为在attr这一列它并没有设置u标志位
0000000000003000 0000000087f5b000 0000000000001000 rwxu-ad  # bss / heap start
0000003fffffe000 0000000087f6f000 0000000000001000 rw---ad  # trapframe page, no u tag, so user code can't access it 
0000003ffffff000 0000000080007000 0000000000001000 r-x--a-  # trampoline page

# So the 6 pages are:
# - ELF program’s code & data (first 4 pages)
# - Trapframe (one page near top of VA space)
# - Trampoline (one page at very top of VA space)


# Each line is a virtual memory region that has a mapping in the page table. It shows:
# - vaddr: virtual start
# - paddr: physical start
# - size: size of mapping (always 4 KB in xv6)
# - attr: permissions (r, w, x, u = user, a = accessed, d = dirty)
# So your output shows just 6 virtual pages mapped.

# What the shell has at exec() time
# When exec("sh") runs, xv6 builds the process address space like this:
# - code (text): loaded from the ELF binary into the first page(s)
# - data: initialized variables and heap start
# - stack: one page at the top of user space (below MAXVA)
# - trampoline: mapped at the very top virtual address (TRAMPOLINE)
# - trapframe page: just below the trampoline, to hold user register state
# That’s it. Shell hasn’t malloc’d or forked much memory yet, so the memory footprint is tiny.



(gdb) x/6i 0xdfa
# (gdb) x/3i $pc
=> 0xdfa <write>:       li      a7,16
   0xdfc <write+2>:     ecall
   0xe00 <write+6>:     ret
   0xe02 <close>:       li      a7,21
   0xe04 <close+2>:     ecall
   0xe08 <close+6>:     ret

(gdb) stepi
(gdb) x/3i 0xdfa
   0xdfa <write>:       li      a7,16
=> 0xdfc <write+2>:     ecall
   0xe00 <write+6>:     ret

# 现在 GDB 在 ecall 前停下（用户态）, 0xdfc     # address of ecall in write
(gdb) p/x $pc
$1 = 0xdfc

(gdb) p/x $sepc
$2 = 0xe08

(gdb) p/x $stvec
$1 = 0x3ffffff000

(gdb) p/x $stap
$3 = 0x0

# breakpoint at  0x3ffffff000 and set riscv use-compressed-breakpoints yes in .gdbinit
# set a breakpoint at the trampoline page address
(gdb) b *$stvec
# (gdb) b *0x3ffffff000    # Common xv6 trampoline virtual address

# (gdb) b uservec         # Trampoline entry
# (gdb) break usertrap        # After trampoline
# (gdb) break sys_write       # Syscall handler
# (gdb) break userret         # Return trampoline

# print physicial addresss at satp register 
# 它并没有告诉我们有关page table中的映射关系是什么，page table长什么样。但是幸运的是，在QEMU中有一个方法可以打印当前的page table。从QEMU界面，输入ctrl a + c可以进入到QEMU的console，之后输入info mem，QEMU会打印完整的page table。
(gdb) p/x $satp
$2 = 0x8000000000087f63

```

## Debug ecall (kernel/trampoline.S)
```bash
# 单条指令执行 ecall，让 CPU 进入 trap（trampoline 将被映射并执行）：
(gdb) stepi
Breakpoint 2, 0x0000003ffffff000 in ?? ()
=> 0x0000003ffffff000:  73 15 05 14     csrrw   a0,sscratch,a0
# stepi 执行 ecall，QEMU 会触发 trap，trampoline 页被映射到地址空间
# xv6 的 trampoline/trap 入口通常有符号名/ 地址：
# trampoline 是用户态 → 内核态的入口。要 hit 它，必须有一个用户进程执行 ecall。


# 此时你可以用 x/20i $pc、info registers、继续 stepi 来逐条看 trampoline / 用户 trap handler 的指令。

# we are the start of the trampoline page
(gdb) p/x $pc
$2 = 0x3ffffff000

# ecall just the address of stvec regester point to : the begging of trampoline page
(gdb) p/x $stvec
$2 = 0x3ffffff000

# When a trap occurs, RISC-V saves the program counter to sepc register.
(gdb) p/x $sepc
$6 = 0xdfc

# sscratch point to the address of trampframe
(gdb) p/x $sscratch
$1 = 0x3fffffe000


(gdb) x/10i $pc
=> 0x3ffffff000:        csrrw   a0,sscratch,a0
   0x3ffffff004:        sd      ra,40(a0)
   0x3ffffff008:        sd      sp,48(a0)
   0x3ffffff00c:        sd      gp,56(a0)
   0x3ffffff010:        sd      tp,64(a0)
   0x3ffffff014:        sd      t0,72(a0)
   0x3ffffff018:        sd      t1,80(a0)
   0x3ffffff01c:        sd      t2,88(a0)
   0x3ffffff020:        sd      s0,96(a0)
   0x3ffffff022:        sd      s1,104(a0)

# what to do next? (kernel/trampoline.S)
# - save 32 user registers to trampframe: struct trampframe in kernel/proc.h
# - switch from user page table to kernel page table
# - create/find a kernel stack so we can run C code 


(gdb) p/x $satp
$12 = 0x0

(gdb) x/10i $pc
=> 0x3ffffff000:        csrrw   a0,sscratch,a0 # swap a0 with $sscratch register
   0x3ffffff004:        sd      ra,40(a0)
   0x3ffffff008:        sd      sp,48(a0)
   0x3ffffff00c:        sd      gp,56(a0)
   0x3ffffff010:        sd      tp,64(a0)
   0x3ffffff014:        sd      t0,72(a0)
   0x3ffffff018:        sd      t1,80(a0)
   0x3ffffff01c:        sd      t2,88(a0)
   0x3ffffff020:        sd      s0,96(a0)
   0x3ffffff022:        sd      s1,104(a0)

(gdb) si

# after swap a0 with $sscratch register
(gdb) p/x $a0
$10 = 0x3fffffe000

(gdb) p/x $sscratch
$11 = 0x2

# cast to trapframe at $a0
(gdb) p/x *(struct trapframe*)$a0
$18 = {kernel_satp = 0x8000000000087fff, kernel_sp = 0x3fffffe000, kernel_trap = 0x800029ee, 
  epc = 0x0, kernel_hartid = 0x0, ra = 0x505050505050505, sp = 0x1000, gp = 0x505050505050505, 
  tp = 0x505050505050505, t0 = 0x505050505050505, t1 = 0x505050505050505, t2 = 0x505050505050505, 
  s0 = 0x505050505050505, s1 = 0x505050505050505, a0 = 0x505050505050505, a1 = 0x505050505050505, 
  a2 = 0x505050505050505, a3 = 0x505050505050505, a4 = 0x505050505050505, a5 = 0x505050505050505, 
  a6 = 0x505050505050505, a7 = 0x505050505050505, s2 = 0x505050505050505, s3 = 0x505050505050505, 
  s4 = 0x505050505050505, s5 = 0x505050505050505, s6 = 0x505050505050505, s7 = 0x505050505050505, 
  s8 = 0x505050505050505, s9 = 0x505050505050505, s10 = 0x505050505050505, s11 = 0x505050505050505, 
  t3 = 0x505050505050505, t4 = 0x505050505050505, t5 = 0x505050505050505, t6 = 0x505050505050505}

(gdb) where
#0  0x0000003ffffff07a in ?? ()
# in trampoline.S
# # restore kernel stack pointer from p->trapframe->kernel_sp
# ld sp, 8(a0)
# # make tp hold the current hartid, from p->trapframe->kernel_hartid
# ld tp, 32(a0)
# # load the address of usertrap(), p->trapframe->kernel_trap
# ld t0, 16(a0)


# ld t0, 16(a0)
# │  >0x3ffffff07a        ld  tp,32(a0)
# │   0x3ffffff07e        ld  t0,16(a0)
# │   0x3ffffff082        ld  t1,0(a0)


(gdb) si
0x0000003ffffff076 in ?? ()
=> 0x0000003ffffff076:  03 31 85 00     ld      sp,8(a0)
(gdb) p/x $a0
$20 = 0x3fffffe000
(gdb) si
0x0000003ffffff07a in ?? ()
=> 0x0000003ffffff07a:  03 32 05 02     ld      tp,32(a0)
(gdb) si
0x0000003ffffff07e in ?? ()
=> 0x0000003ffffff07e:  83 32 05 01     ld      t0,16(a0)

(gdb) p/x $a0
$20 = 0x3fffffe000

#  sp points to the kenel stack 
(gdb) p/x $sp
$21 = 0x3fffffc000

#  xv6 keeps the core number called hart id in the tp register
(gdb) p/x $tp
$25 = 0x0

# restore kernel page table from p->trapframe->kernel_satp
# ld t1, 0(a0)
# csrw satp, t1
# sfence.vma zero, zero

# load the kernel page table and flush the TLB
(gdb) si
0x0000003ffffff08a in ?? ()
=> 0x0000003ffffff08a:  73 00 00 12     sfence.vma

(qemu) info mem
vaddr            paddr            size             attr
---------------- ---------------- ---------------- -------
0000000002000000 0000000002000000 0000000000010000 rw-----
000000000c000000 000000000c000000 0000000000001000 rw---ad
000000000c001000 000000000c001000 0000000000001000 rw-----
000000000c002000 000000000c002000 0000000000001000 rw---ad
000000000c003000 000000000c003000 00000000001fe000 rw-----
000000000c201000 000000000c201000 0000000000001000 rw---ad
000000000c202000 000000000c202000 00000000001fe000 rw-----
0000000010000000 0000000010000000 0000000000002000 rw---ad
0000000080000000 0000000080000000 0000000000007000 r-x--a-
0000000080007000 0000000080007000 0000000000001000 r-x----
0000000080008000 0000000080008000 0000000000003000 rw---ad
000000008000b000 000000008000b000 0000000000006000 rw-----
0000000080011000 0000000080011000 0000000000011000 rw---ad
0000000080022000 0000000080022000 0000000000001000 rw-----
0000000080023000 0000000080023000 0000000000003000 rw---ad
0000000080026000 0000000080026000 0000000007f35000 rw-----
0000000087f5b000 0000000087f5b000 000000000005d000 rw---ad
0000000087fb8000 0000000087fb8000 0000000000001000 rw---a-
0000000087fb9000 0000000087fb9000 0000000000046000 rw-----
0000000087fff000 0000000087fff000 0000000000001000 rw---a-
0000003ffff7f000 0000000087f77000 000000000003e000 rw-----
0000003fffffb000 0000000087fb5000 0000000000002000 rw---ad
0000003ffffff000 0000000080007000 0000000000001000 r-x--a-

(gdb) x/9i $pc-32
   0x3ffffff06e:        csrr    t0,sscratch
   0x3ffffff072:        sd      t0,112(a0)
   0x3ffffff076:        ld      sp,8(a0)
   0x3ffffff07a:        ld      tp,32(a0)
   0x3ffffff07e:        ld      t0,16(a0)
   0x3ffffff082:        ld      t1,0(a0)
   0x3ffffff086:        csrw    satp,t1
   0x3ffffff08a:        sfence.vma
=> 0x3ffffff08e:        jr      t0

# # jump to usertrap(), which does not return
# jr t0
(gdb) p/x $t0
$28 = 0x800029ee
(gdb) x $t0
0x800029ee <usertrap>:  0xec061101
(gdb) x/4i $t0
   0x800029ee <usertrap>:       addi    sp,sp,-32
   0x800029f0 <usertrap+2>:     sd      ra,24(sp)
   0x800029f2 <usertrap+4>:     sd      s0,16(sp)
   0x800029f4 <usertrap+6>:     sd      s1,8(sp)

(gdb) si
usertrap () at kernel/trap.c:38

(gdb) x/4i $pc
=> 0x800029ee <usertrap>:       addi    sp,sp,-32
   0x800029f0 <usertrap+2>:     sd      ra,24(sp)
   0x800029f2 <usertrap+4>:     sd      s0,16(sp)
   0x800029f4 <usertrap+6>:     sd      s1,8(sp)

```
## Debug usetrap (kernel/trap.c)

```bash
(gdb) b usetrap


(gdb) c
Continuing.
Breakpoint 3, usertrap () at kernel/trap.c:38
38      {

(gdb) layout src

(gdb) p/x $sepc
$6 = 0xdfc
#  // save user program counter.                                      │
# │  >51            p->trapframe->epc = r_sepc(); 

(gdb) p/x $scause
$2 = 0x8
# >53            if(r_scause() == 8){                                                                                    │
# │   54              // system call     

# │   59              // sepc points to the ecall instruction,                                                              │
# │   60              // but we want to return to the next instruction.                                                     │
# │   61              p->trapframe->epc += 4;  

# >67              syscall();
```


## Debug syscall (kenel/syscall.c)

```bash
#  171           struct proc *p = myproc();
#  172  
# >173           num = p->trapframe->a7;   // syscall number from user 
#  175         p->trapframe->a0 = syscalls[num](); // return value in a0

# retrival the syscall number from the trampframe
# SYS_write num is  16
(gdb) p num
$3 = 16

(gdb) print p->trapframe->a0
$5 = 2
(gdb) print p->trapframe->a1
$6 = 16031
(gdb) print p->trapframe->a2
$7 = 1

```

## Debug sys_write(kernel/sysfile.c)

```bash
(gdb) step
sys_write () at kernel/sysfile.c:91
# >91            if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0) 


(gdb) finish
Run till exit from #0  sys_write () at kernel/sysfile.c:91
0x0000000080002d64 in syscall () at kernel/syscall.c:175
175         p->trapframe->a0 = syscalls[num](); // return value in a0
Value returned is $8 = 1

#  return to syscall
# >175             p->trapframe->a0 = syscalls[num](); // return value in a0   
(gdb) print p->trapframe->a0
$9 = 1

(gdb) next

# return to usertrap
(gdb) where
#0  usertrap () at kernel/trap.c:83
#1  0x0000000000000e94 in putc (fd=<optimized out>, c=<optimized out>) at user/printf.c:12
Backtrace stopped: previous frame inner to this frame (corrupt stack?)

#   >83            usertrapret(); 


```

## Debug usertrapret(kernel/trap.c)


# Debug xv6 初始进程 地址空间 with gdb
- [Xv6 代码导读 (调试工具配置；调试系统调用执行) [南京大学2022操作系统-P18]](https://www.bilibili.com/video/BV1DY4y1a7YD?spm_id_from=333.788.videopod.sections&vd_source=b3d4057adb36b9b243dc8d7a6fc41295)
- https://jyywiki.cn/OS/2022/slides/18.slides.html#/

### the start of first process
```c
// kernel/main.c 
main() -> userinit()

// in kernel/proc.c userinit()
// allocate one user page and copy init's  instructions and data into it.
uvminit(p->pagetable, initcode, sizeof(initcode));

// in kernel/proc.c  initcode
// a user program that calls exec("/init") od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// user/init.c : The initial user-level program

```

### Debug the first process
```bash
cd xv6-riscv
make qemu-gdb 


gdb-multiarch kernel/kernel

(gdb) b *0
Breakpoint 1 at 0x0
(gdb) c
Continuing.

Breakpoint 1, 0x0000000000000000 in ?? ()
=> 0x0000000000000000:  17 05 00 00     auipc   a0,0x0

# 系统调用实现，编号放入a7 寄存器，执行 ecall 指令
(gdb) x/10i 0
=> 0x0: auipc   a0,0x0
   0x4: addi    a0,a0,36
   0x8: auipc   a1,0x0
   0xc: addi    a1,a1,35
   0x10:        li      a7,7
   0x14:        ecall
   0x18:        li      a7,2
   0x1c:        ecall
   0x20:        jal     ra,0x18
   0x24:        0x696e692f

(gdb) si
0x0000000000000014 in ?? ()
=> 0x0000000000000014:  73 00 00 00     ecall

(gdb) p $pc
$2 = (void (*)()) 0x14
(gdb) x $stvec
   0x3ffffff000:        Cannot access memory at address 0x3ffffff000

# 查看地址空间
(qemu) Gdk-Message: 07:28:36.529: Unable to load hand2 from the cursor theme
info mem
vaddr            paddr            size             attr
---------------- ---------------- ---------------- -------
0000000000000000 0000000087f72000 0000000000001000 rwxu-a-
0000003fffffe000 0000000087f76000 0000000000001000 rw---a-
0000003ffffff000 0000000080007000 0000000000001000 r-x--a-

(gdb) b *$stvec
Cannot access memory at address 0x3ffffff000

# get into ecall, get into trampoline.S
(gdb) si
Breakpoint 3, 0x0000003ffffff000 in ?? ()
=> 0x0000003ffffff000:  73 15 05 14     csrrw   a0,sscratch,a0

# sscratch(S-mode scratch register) 用来保存所有的寄存器 到 trampframe
(gdb) x/10i $pc
=> 0x3ffffff000:        csrrw   a0,sscratch,a0
   0x3ffffff004:        sd      ra,40(a0)
   0x3ffffff008:        sd      sp,48(a0)
   0x3ffffff00c:        sd      gp,56(a0)
   0x3ffffff010:        sd      tp,64(a0)
   0x3ffffff014:        sd      t0,72(a0)
   0x3ffffff018:        sd      t1,80(a0)
   0x3ffffff01c:        sd      t2,88(a0)
   0x3ffffff020:        sd      s0,96(a0)
   0x3ffffff022:        sd      s1,104(a0)

# after load the satp register
(gdb) x/10i $pc-16
   0x3ffffff07e:        ld      t0,16(a0)
   0x3ffffff082:        ld      t1,0(a0)
   0x3ffffff086:        csrw    satp,t1
   0x3ffffff08a:        sfence.vma
=> 0x3ffffff08e:        jr      t0
   0x3ffffff090:        csrw    satp,a1
   0x3ffffff094:        sfence.vma
   0x3ffffff098:        ld      t0,112(a0)
   0x3ffffff09c:        csrw    sscratch,t0
   0x3ffffff0a0:        ld      ra,40(a0)

# 进入kernel 空间， the kernel page is loaded
(qemu) info mem
vaddr            paddr            size             attr
---------------- ---------------- ---------------- -------
0000000002000000 0000000002000000 0000000000010000 rw-----
000000000c000000 000000000c000000 0000000000001000 rw---ad
000000000c001000 000000000c001000 0000000000001000 rw-----
000000000c002000 000000000c002000 0000000000001000 rw---ad
000000000c003000 000000000c003000 00000000001fe000 rw-----
000000000c201000 000000000c201000 0000000000001000 rw---ad
000000000c202000 000000000c202000 00000000001fe000 rw-----
0000000010000000 0000000010000000 0000000000001000 rw---a-
0000000010001000 0000000010001000 0000000000001000 rw---ad
0000000080000000 0000000080000000 0000000000007000 r-x--a- # kernel 
0000000080007000 0000000080007000 0000000000001000 r-x----
0000000080008000 0000000080008000 0000000000003000 rw---ad
000000008000b000 000000008000b000 0000000000006000 rw-----
0000000080011000 0000000080011000 0000000000011000 rw---ad
0000000080022000 0000000080022000 0000000000001000 rw-----
0000000080023000 0000000080023000 0000000000003000 rw---ad
0000000080026000 0000000080026000 0000000007f4a000 rw-----
0000000087f70000 0000000087f70000 0000000000048000 rw---ad
0000000087fb8000 0000000087fb8000 0000000000001000 rw---a-
0000000087fb9000 0000000087fb9000 0000000000046000 rw-----
0000000087fff000 0000000087fff000 0000000000001000 rw---a-
0000003ffff7f000 0000000087f77000 000000000003f000 rw-----
0000003fffffd000 0000000087fb6000 0000000000001000 rw---ad
0000003ffffff000 0000000080007000 0000000000001000 r-x--a-


(gdb) x/15i $pc-48
   0x3ffffff05e:        sd      t3,256(a0)
   0x3ffffff062:        sd      t4,264(a0)
   0x3ffffff066:        sd      t5,272(a0)
   0x3ffffff06a:        sd      t6,280(a0)
   0x3ffffff06e:        csrr    t0,sscratch
   0x3ffffff072:        sd      t0,112(a0)
   0x3ffffff076:        ld      sp,8(a0)
   0x3ffffff07a:        ld      tp,32(a0)
   0x3ffffff07e:        ld      t0,16(a0)
   0x3ffffff082:        ld      t1,0(a0)
   0x3ffffff086:        csrw    satp,t1
   0x3ffffff08a:        sfence.vma
=> 0x3ffffff08e:        jr      t0
   0x3ffffff090:        csrw    satp,a1
   0x3ffffff094:        sfence.vma

# 准备跳转到 kernel/trap.c usertrap
(gdb) x $t0
   0x800029ee <usertrap>:       addi    sp,sp,-32

(gdb) si
usertrap () at kernel/trap.c:38

(gdb) where
#0  usertrap () at kernel/trap.c:53
#1  0x0505050505050505 in ?? ()
(gdb) list
# 58
# 59          // sepc points to the ecall instruction,
# 60          // but we want to return to the next instruction.
# 61          p->trapframe->epc += 4;
# 62
# 63          // an interrupt will change sstatus &c registers,
# 64          // so don't enable until done with those registers.
# 65          intr_on();
# 66
# 67          syscall();

(gdb) p p
$5 = (struct proc *) 0x80011d68 <proc>

(gdb) p *p
$9 = {lock = {locked = 0, name = 0x80008210 "proc", cpu = 0x0}, 
  state = RUNNING, parent = 0x0, chan = 0x0, killed = 0, xstate = 0, 
  pid = 1, kstack = 274877894656, sz = 4096, pagetable = 0x87f75000, 
  kpagetable = 0x0, trapframe = 0x87f76000, context = {
    ra = 2147492328, sp = 274877898352, s0 = 274877898400, 
    s1 = 2147556712, s2 = 2147555664, s3 = 1, s4 = 2147635200, 
    s5 = 2147627008, s6 = 8192, s7 = 2147627008, s8 = 8, s9 = 4, 
    s10 = 1, s11 = 0}, ofile = {0x0 <repeats 16 times>}, 
  cwd = 0x80020278 <icache+24>, 
  name = "initcode\000\000\000\000\000\000\000", tracemask = 0}

(gdb) p p->trapframe
$10 = (struct trapframe *) 0x87f76000

(gdb) p *p->trapframe
$11 = {kernel_satp = 9223372036855332863, kernel_sp = 274877898752, 
  kernel_trap = 2147494382, epc = 20, kernel_hartid = 0, 
  ra = 361700864190383365, sp = 4096, gp = 361700864190383365, 
  tp = 361700864190383365, t0 = 361700864190383365, 
  t1 = 361700864190383365, t2 = 361700864190383365, 
  s0 = 361700864190383365, s1 = 361700864190383365, a0 = 36, a1 = 43, 
  a2 = 361700864190383365, a3 = 361700864190383365, 
  a4 = 361700864190383365, a5 = 361700864190383365, 
  a6 = 361700864190383365, a7 = 7, s2 = 361700864190383365, 
  s3 = 361700864190383365, s4 = 361700864190383365, 
  s5 = 361700864190383365, s6 = 361700864190383365, 
  s7 = 361700864190383365, s8 = 361700864190383365, 
  s9 = 361700864190383365, s10 = 361700864190383365, 
  s11 = 361700864190383365, t3 = 361700864190383365, 
  t4 = 361700864190383365, t5 = 361700864190383365, 
  t6 = 361700864190383365}

(gdb) p p->trapframe->epc
$13 = 20


(gdb) where
#0  usertrap () at kernel/trap.c:67
#1  0x0505050505050505 in ?? ()
(gdb) list
# 62
# 63          // an interrupt will change sstatus &c registers,
# 64          // so don't enable until done with those registers.
# 65          intr_on();
# 66
# 67          syscall();
# 68        } else if((which_dev = devintr()) != 0){
# 69          // ok
# 70        } else {
# 71          printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
# (gdb) p p->trapframe->epc
# $17 = 24


(gdb) s
syscall () at kernel/syscall.c:171
171       struct proc *p = myproc();
(gdb) where
#0  syscall () at kernel/syscall.c:171
#1  0x0000000080002a4c in usertrap () at kernel/trap.c:67
#2  0x0505050505050505 in ?? ()
(gdb) list
166
167     void
168     syscall(void)
169     {
170       int num;
171       struct proc *p = myproc();
172
173       num = p->trapframe->a7;   // syscall number from user
174       if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
175         p->trapframe->a0 = syscalls[num](); // return value in a0
(gdb) 


```

# Switch Page Table When traps

Note that the CPU doesn’t switch to the kernel page table, doesn’t switch to a stack in the kernel, and doesn’t save any registers other than the pc. 
Kernel software must perform these tasks.
One reason that the CPU does minimal work during a trap is to provide flexibility to software;
for example, some operating systems omit a page table switch in some situations to increase trap performance.


It’s worth thinking about whether any of the steps listed above could be omitted, perhaps in search of faster traps. Though there are situations in which a simpler sequence can work, many of the steps would be dangerous to omit in general. 
For example, suppose that the CPU didn’t switch program counters. Then a trap from user space could switch to supervisor mode while still running user instructions. Those user instructions could break user/kernel isolation, 

## `satp register`


for example by modifying the `satp register` to point to a page table that allowed accessing all of physical memory.
It is thus important that the CPU switch to a kernel-specified instruction address, namely `stvec`.

the **`satp` register** is one of the most important registers in RISC-V virtual memory, and xv6 relies on it heavily for switching between user and kernel address spaces. Let’s unpack it.

---

### 1. What is `satp`?

* `satp` = **Supervisor Address Translation and Protection** register.
* 指向page table的物理内存地址
* It controls **virtual memory translation** when the CPU is running in supervisor mode (S-mode, which xv6 kernel runs in).
* Specifically, it tells the hardware:

  1. **Which page table to use** (the root of the page table tree).
  2. **Which translation mode to use** (bare physical addresses vs. paging): tells hardware how to do virtual→physical translation
  3. (Optional) ASID (address space identifier), though xv6 doesn’t use this.

---

### 2. The structure of `satp` (Sv39 mode used by xv6-riscv)

On 64-bit RISC-V with Sv39 (3-level page tables), `satp` is laid out like this:

```
63          60 59                       44 43                          0
+--------------+---------------------------+-----------------------------+
| MODE (4 bits)|     ASID (16 bits)        |    PPN (44 bits)            |
+--------------+---------------------------+-----------------------------+
```

* **MODE**

  * `0` = Bare (no paging, virtual = physical).
  * `8` = Sv39 (39-bit virtual addresses, 3-level page tables). ← used by xv6.
* **ASID**: Address space ID (for TLB tagging). xv6 doesn’t use it → just 0.
* **PPN**: Physical Page Number of the **root page table** (the `pagetable` you see in xv6).

So:
👉 Writing to `satp` tells the hardware, *"From now on, use this physical address as the root of the current page table tree, and interpret virtual addresses accordingly."*

---

### 3. How xv6 uses `satp`

* **Global kernel page table**:
  At boot, xv6 sets up a kernel page table (`kernel_pagetable`) and loads it into `satp`.

  ```c
  extern pagetable_t kernel_pagetable;
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
  ```

  From then on, all memory accesses go through this page table.

* **User process page table**:
  Each process has its own `pagetable`. When xv6 switches into user mode, it writes that process’s page table root into `satp`.
  When the scheduler switches processes, it reloads `satp` with the new process’s page table.

* **Switching kernel page tables (advanced labs)**:
  In the per-process kernel page table exercise, you modify xv6 so that *even in the kernel*, `satp` points to a process-specific kernel page table. That’s also just reloading `satp`.

---

### 4. Key supporting instructions

* **`w_satp(x)`**: Write a new value into `satp`. This switches page tables.
* **`sfence.vma`**: Flushes the TLB (translation cache). Needed after changing `satp` so stale mappings don’t get used.

---

### 5. Analogy

Think of `satp` as a **pointer to the current "map of memory"**:

* The OS keeps many maps (page tables), one per process.
* To run a process, the kernel just says to the CPU: *“Use this map!”* → by writing the root address into `satp`.
* The CPU then interprets all loads/stores/jumps according to that map.

---

✅ **Summary**:
The `satp` register in xv6 tells the CPU *which page table root to use*. By writing the physical address of a page table into `satp` and setting the mode to Sv39, xv6 enables virtual memory translation. Context switches and kernel/user transitions rely on saving and restoring `satp` to point to the right page table.

---


### The Exact lines in xv6 where `satp` is loaded during process switch

Perfect, let’s walk through xv6-riscv code and find the **exact lines where the `satp` register is written during a process switch**.

---

#### 1. Where `satp` is written

In xv6, the write is done by the helper `w_satp()` (defined in `kernel/riscv.h`):

```c
static inline void
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}
```

That’s the low-level inline assembly that loads the new root page table into the `satp` register.

---

#### 2. Switching to a user process (trap return)

The most important place is in **`kernel/trampoline.S` → `userret`**, called from `usertrapret()` in `trap.c`.

### `kernel/trap.c`:

```c
void
usertrapret(void)
{
  struct proc *p = myproc();

  // set up to return to user space
  w_satp(MAKE_SATP(p->pagetable));
  sfence_vma();
  ...
}
```

Here, when the kernel is about to return to user mode, it loads the process’s **user page table root** into `satp`.
That means once the CPU executes `sret`, the user’s virtual addresses are valid.

---

#### 3. Scheduler context switch

When the scheduler switches processes, it saves and restores context.

In **`kernel/proc.c` → `scheduler()`**, you’ll see:

```c
for(;;){
  ...
  // switch to chosen process’s address space
  p->state = RUNNING;

  // load the process’s page table
  w_satp(MAKE_SATP(p->pagetable));
  sfence_vma();

  swtch(&c->scheduler, p->context);
  ...
}
```

This is where the scheduler tells the CPU:
 “Okay, from now on, use this process’s page table.”

After that, the process runs in its own memory space.

---

#### 4. Switching back to kernel page table

When no process is running (e.g., idle loop in scheduler), xv6 switches back to the **global kernel page table**:

```c
w_satp(MAKE_SATP(kernel_pagetable));
sfence_vma();
```

So the kernel itself always has a valid page table to work with, even when not running any user process.

---

#### 5. Summary of key points

* **`w_satp()`** is the primitive that writes to the `satp` register.

* It is called in two important contexts:

  1. **`usertrapret()`** → before returning to user space, set `satp = p->pagetable`.
  2. **`scheduler()`** → before running a process, set `satp = p->pagetable`.

* Always followed by **`sfence_vma()`** to flush the TLB.

---

In the **per-process kernel page table lab** you mentioned earlier, you’ll extend this so that `scheduler()` and `usertrapret()` will load **per-process kernel page tables** instead of the single global `kernel_pagetable`.

---

Do you want me to also trace **where xv6 switches from user page table back to kernel page table** (e.g., on a trap/interrupt)?


# 4.3 Code: Calling system calls

1. `initcode.S` places the `arguments` for exec in `registers a0 and a1`, and puts the `system call number in a7`. 
2. System call numbers match the entries in the syscalls array, a table of function pointers (kernel/syscall.c:108). 
3. The `ecall` instruction traps into the kernel and causes `uservec`, `usertrap`, and then syscall to execute, as we saw above.
4. `syscall` (kernel/syscall.c:133) retrieves the system call number from the saved a7 in the trapframe and uses it to index into syscalls. 
For the first system call, a7 contains `SYS_exec` (kernel/syscall.h:8), resulting in a call to the system call implementation function `sys_exec`.
1. When sys_exec returns, syscall records its return value in `p->trapframe->a0`. 
This will cause the original user-space call to `exec()` to return that value, since the C calling convention on RISC-V places return values in a0. 
System calls conventionally return negative numbers to indicate errors, and zero or positive numbers for success. If the system call number is invalid, syscall prints an error and returns -1.

# 4.4 Code: System call arguments


## Parameter handling
- `argint` to retrieve the integer argument
- `argfd()` to retrieve the file descriptor argument
- `argaddr()` to get the user-space pointer for the stat structure
  

System call implementations in the kernel need to find the arguments passed by user code. 
Because user code calls `system call wrapper functions`, the arguments are initially where the RISC-V Calling convention places them: `in registers`. 

`The kernel trap code` saves user registers to the current process’s `trap frame`, where kernel code can find them. 
The kernel functions `argint`, `argaddr`, and `argfd` retrieve the n’th system call argument from the `trap frame` as an integer, pointer, or a file descriptor. 
They all call `argraw` to retrieve the appropriate saved user register (kernel/syscall.c:35).
Some system calls pass pointers as arguments, and the kernel must use those pointers to read or write user memory. 

The exec system call, for example, passes the kernel an array of pointers referring to string arguments in user space. These pointers pose two challenges. 
- First, the user program may be buggy or malicious, and may pass the kernel an invalid pointer or a pointer intended to trick the kernel into accessing kernel memory instead of user memory. 
- Second, the xv6 kernel page table mappings are not the same as the user page table mappings, so the kernel cannot use ordinary instructions to load or store from user-supplied addresses.


## Safely transfer data to and from user-supplied addresses
- `fetchstr`
- `copyinstr`
- `walkaddr`
- `copyout`

The kernel implements functions that safely transfer data to and from user-supplied addresses.
`fetchstr` is an example (kernel/syscall.c:25).0
File system calls such as exec use fetchstr to retrieve string file-name arguments from user space. 
fetchstr calls `copyinstr` to do the hard work.

`copyinstr` (kernel/vm.c:398) copies up to max bytes to dst from virtual address srcva in the user page table pagetable. 
Since pagetable is not the current page table, copyinstr uses `walkaddr` (which calls walk) to look up srcva in pagetable, yielding physical address pa0. 

The kernel maps each physical RAM address to the corresponding kernel virtual address, so copyinstr can directly copy string bytes from pa0 to dst. 
walkaddr (kernel/vm.c:104) checks that the user-supplied virtual address is part of the process’s user address space, so programs cannot trick the kernel into reading other memory. 
A similar function, `copyout`, copies data from the kernel to a user-supplied address.


# 4.5 Traps from kernel space


Xv6 configures the CPU trap registers somewhat differently depending on whether user or kernel code is executing. 
When the kernel is executing on a CPU, the kernel points `stvec` to the assembly code at `kernelvec` (kernel/kernelvec.S:10). 
Since xv6 is already in the kernel, kernelvec can rely on `satp` being set to the kernel page table, and on the stack pointer referring to a valid kernel
stack. 
kernelvec pushes all 32 registers onto the stack, from which it will later restore them so that the interrupted kernel code can resume without isturbance.
kernelvec saves the registers on the stack of the interrupted kernel thread, which makes sense because the register values belong to that thread. This is particularly important if the trap causes a switch to a different thread – in that case the trap will actually return from the stack of the new thread, leaving the interrupted thread’s saved registers safely on its stack.

`kernelvec` jumps to `kerneltrap` (kernel/trap.c:134) after saving registers. 
kerneltrap is prepared for two types of traps: device interrrupts and exceptions. It calls `devintr` (kernel/-trap.c:177) to check for and handle the former. If the trap isn’t a device interrupt, it must be an exception, and that is always a fatal error if it occurs in the xv6 kernel; the kernel calls panic and stops executing.
If kerneltrap was called due to a timer interrupt, and a process’s kernel thread is running (as opposed to a scheduler thread), kerneltrap calls yield to give other threads a chance to run. At some point one of those threads will yield, and let our thread and its kerneltrap resume again. 

Chapter 7 explains what happens in yield.
When kerneltrap’s work is done, it needs to return to whatever code was interrupted by the trap. Because a yield may have disturbed sepc and the previous mode in sstatus, kerneltrap saves them when it starts. It now restores those control registers and returns to kernelvec (kernel/kernelvec.S:48). kernelvec pops the saved registers from the stack and executes sret, which copies sepc to pc and resumes the interrupted kernel code.
It’s worth thinking through how the trap return happens if kerneltrap called yield due to a timer interrupt.
Xv6 sets a CPU’s stvec to kernelvec when that CPU enters the kernel from user space;
you can see this in usertrap (kernel/trap.c:29). There’s a window of time when the kernel has
started executing but stvec is still set to uservec, and it’s crucial that no device interrupt occur
during that window. Luckily the RISC-V always disables interrupts when it starts to take a trap,
and xv6 doesn’t enable them again until after it sets stvec.