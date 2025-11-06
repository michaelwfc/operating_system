
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
- stvec is a RISC-V control register (CSR) that tells the CPU where to jump when a trap (exception or interrupt) happens while in S-mode (supervisor/kernel mode).

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




### `satp register`


the **`satp` register** is one of the most important registers in RISC-V virtual memory, and xv6 relies on it heavily for switching between user and kernel address spaces. Let’s unpack it.

---

#### 1. What is `satp`?

* `satp` = **Supervisor Address Translation and Protection** register.
* 指向page table的物理内存地址
* It controls **virtual memory translation** when the CPU is running in supervisor mode (S-mode, which xv6 kernel runs in).
* Specifically, it tells the hardware:

  1. **Which page table to use** (the root of the page table tree).
  2. **Which translation mode to use** (bare physical addresses vs. paging): tells hardware how to do virtual→physical translation
  3. (Optional) ASID (address space identifier), though xv6 doesn’t use this.

---

#### 2. The structure of `satp` (Sv39 mode used by xv6-riscv)

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

#### 3. How xv6 uses `satp`

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

#### 4. Key supporting instructions

* **`w_satp(x)`**: Write a new value into `satp`. This switches page tables.
* **`sfence.vma`**: Flushes the TLB (translation cache). Needed after changing `satp` so stale mappings don’t get used.

---

#### 5. Analogy

Think of `satp` as a **pointer to the current "map of memory"**:

* The OS keeps many maps (page tables), one per process.
* To run a process, the kernel just says to the CPU: *“Use this map!”* → by writing the root address into `satp`.
* The CPU then interprets all loads/stores/jumps according to that map.

---

✅ **Summary**:
The `satp` register in xv6 tells the CPU *which page table root to use*. By writing the physical address of a page table into `satp` and setting the mode to Sv39, xv6 enables virtual memory translation. Context switches and kernel/user transitions rely on saving and restoring `satp` to point to the right page table.

---


#### The Exact lines in xv6 where `satp` is loaded during process switch

Perfect, let’s walk through xv6-riscv code and find the **exact lines where the `satp` register is written during a process switch**.

---

##### 1. Where `satp` is written

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

##### 2. Switching to a user process (trap return)

The most important place is in **`kernel/trampoline.S` → `userret`**, called from `usertrapret()` in `trap.c`.

`kernel/trap.c`:

```c
void
usertrapret(void)
{
  struct proc *p = myproc();

  // When xv6 switches to this process, it loads the physical address of the root page table into the RISC-V satp register:
  // set up to return to user space
  w_satp(MAKE_SATP(p->pagetable));
  sfence_vma();
  ...
}
```

Here, when the kernel is about to return to user mode, it loads the process’s **user page table root** into `satp`.
That means once the CPU executes `sret`, the user’s virtual addresses are valid.

---

##### 3. Scheduler context switch

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

##### 4. Switching back to kernel page table

When no process is running (e.g., idle loop in scheduler), xv6 switches back to the **global kernel page table**:

```c
w_satp(MAKE_SATP(kernel_pagetable));
sfence_vma();
```

So the kernel itself always has a valid page table to work with, even when not running any user process.

---

##### 5. Summary of key points

* **`w_satp()`** is the primitive that writes to the `satp` register.

* It is called in two important contexts:

  1. **`usertrapret()`** → before returning to user space, set `satp = p->pagetable`.
  2. **`scheduler()`** → before running a process, set `satp = p->pagetable`.

* Always followed by **`sfence_vma()`** to flush the TLB.

---

In the **per-process kernel page table lab** you mentioned earlier, you’ll extend this so that `scheduler()` and `usertrapret()` will load **per-process kernel page tables** instead of the single global `kernel_pagetable`.

---

Do you want me to also trace **where xv6 switches from user page table back to kernel page table** (e.g., on a trap/interrupt)?


## Switch Page Table When traps

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


Note that the CPU doesn’t switch to the kernel page table, doesn’t switch to a stack in the kernel, and doesn’t save any registers other than the pc. 
Kernel software must perform these tasks.
One reason that the CPU does minimal work during a trap is to provide flexibility to software;
for example, some operating systems omit a page table switch in some situations to increase trap performance.


It’s worth thinking about whether any of the steps listed above could be omitted, perhaps in search of faster traps. Though there are situations in which a simpler sequence can work, many of the steps would be dangerous to omit in general. 
For example, suppose that the CPU didn’t switch program counters. Then a trap from user space could switch to supervisor mode while still running user instructions. Those user instructions could break user/kernel isolation, 


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

## 4.2.1 High-level Picture
```
SH: main()
      |
    rumcmd(cmd)
      |
    exec("echo")
      |
    echo()
      |
    write()
      |
    ecall
----------------
    uservec     (kernel/trampoline.S)    userret
    usertrap()  (kernel/trap.c)         usertrapret
    syscall()
    sys_write()
```


Xv6 handles traps differently depending on whether it is executing in the kernel or in user code.
Here is the story for traps from user code; Section 4.5 describes traps from kernel code.

A trap may occur while executing in user space if the user program makes a system call (`ecall` instruction), or does something illegal, or if a device interrupts. 

The high-level path of a trap from user space is : 
`uservec` (kernel/trampoline.S:16) -> then `usertrap` (kernel/trap.c:37); 

when returning:
`usertrapret` (kernel/trap.c:90)  ->  then `userret` (kernel/trampoline.S:88).


[trap-note](https://mit-public-courses-cn-translatio.gitbook.io/mit6-s081/lec06-isolation-and-system-call-entry-exit-robert/6.1-trap)


## 4.2.2 trampoline(跳板): trampoline assembel code (kernel/trampoline.S)

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

## 4.2.3 trap process from ecall

### 0. ecall: triger trap when write()

when `ecall` instruction is executed
ecall 用 system call 的方式觸發 trap， 提高程序的权限，挑战程序的 trampoline
ecall is an exception instruction that triggers a trap into the kernel

In user space, it doesn’t execute normal code. Instead, the CPU switches to privileged mode, saves the current PC, and jumps to the kernel's trap handler.

From GDB’s perspective, the kernel code is not part of your user-space program (kernel/kernel ELF you loaded). It’s in a different memory space, usually not mapped in your debugging session.

當 trap 發生時，會做的事情：
- sstatus(Supervisor status rigister): 把現在的狀態 (user or supervisor) 紀錄在 sstatus 的 SPP bit
  mode : user mode -> supervisor mode,  我们需要将mode改成 supervisor mode，因为我们想要使用内核中的各种各样的特权指令。
- scause: 把造成 trap 的原因紀錄在 scause
- pc -> sepc: 把 pc 複製到 sepc(Supervisor Exception Program Counter) 中，進入 supervisor mode 後，用來紀錄回到 user mode 時，要回到什麼 address 開始執行
- sscratch: 進入到 kernel space 前 sscratch 會儲存 trapframe 的位置, 用来存储 32 user registers
- stvec->pc: 當 trap 發生時，RISC-V 會把 stvec 放到 pc 中, stvec point to the beginning of the trampoline page
- pc: 開始根據 pc 往下執行 to the start of trampoline page, the very next instruction will be fetched from the trampoline page
- satp: Supervisor Address Translation and Protection Register, No change


get the address of write instruction from user/sh.asm

user/usys.S

```bash
000000000000141e <write>:
.global write
write:
 li a7, SYS_write
    141e:	48c1                	li	a7,16
 ecall
    1420:	00000073          	ecall
 ret
    1424:	8082                	ret


# set a breakpoint at write()
(gdb) b *0x141e 
Breakpoint 1 at 0x141e: file user/usys.S, line 40.

(gdb) c
Continuing.

Breakpoint 1, write () at user/usys.S:40
40       li a7, SYS_write
=> 0x000000000000141e <write+0>:        c1 48   li      a7,16



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
$1 = (void (*)()) 0x141e <write>

# GDB by default only shows general-purpose registers
# On RISC-V, info reg lists:
# - integer registers (ra, sp, gp, tp, t0–t6, s0–s11, a0–a7)
# - pc
# But not CSRs (Control and Status Registers), like satp, sstatus, stvec, etc.

(gdb) info reg

(gdb) info reg
ra             0x2a8    0x2a8 <getcmd+38>
sp             0x3fa0   0x3fa0
gp             0x505050505050505        0x505050505050505
tp             0x505050505050505        0x505050505050505
t0             0x505050505050505        361700864190383365
t1             0x505050505050505        361700864190383365
t2             0x505050505050505        361700864190383365
fp             0x3fc0   0x3fc0
s1             0x505050505050505        361700864190383365
a0             0x2      2
a1             0x1d78   7544
a2             0x2      2
a3             0x505050505050505        361700864190383365
a4             0x3      3
a5             0x64     100
a6             0x505050505050505        361700864190383365
a7             0x15     21
s2             0x505050505050505        361700864190383365
s3             0x505050505050505        361700864190383365
s4             0x505050505050505        361700864190383365
s5             0x505050505050505        361700864190383365
s6             0x505050505050505        361700864190383365
s7             0x505050505050505        361700864190383365
s8             0x505050505050505        361700864190383365
s9             0x505050505050505        361700864190383365
s10            0x505050505050505        361700864190383365
s11            0x505050505050505        361700864190383365
t3             0x505050505050505        361700864190383365
t4             0x505050505050505        361700864190383365
t5             0x505050505050505        361700864190383365
t6             0x505050505050505        361700864190383365
pc             0x141e   0x141e <write>



# the address of user space is quite small/low in xv6
# sp: stack pointer
sp             0x3e90   0x3e90
# pc: program counter
pc             0x141e    0x141e <write>



# examine memory at register a1 with 2 elements:each in character format(byte)
# “show 2 characters starting from the address stored in a1.”
(gdb) x/2c $a1
0x1d78: 36 '$'  32 ' '
# a0: file descriptor to shell argument
# a1: the pointer to the buffer of chacters the shell want to write in a1
# a2: the number of characters to write in a2


# satp register point to the page table
(gdb) p/x $satp
$2 = 0x8000000000087f63

# print physicial addresss at satp register 
# 它并没有告诉我们有关page table中的映射关系是什么，page table长什么样。但是幸运的是，在QEMU中有一个方法可以打印当前的page table。从QEMU界面，输入ctrl a + c可以进入到QEMU的console，之后输入info mem，QEMU会打印完整的page table。

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



# (gdb) x/6i 0x141e
(gdb) x/6i $pc
=> 0x141e <write>:      li      a7,16
   0x1420 <write+2>:    ecall
   0x1424 <write+6>:    ret
   0x1426 <close>:      li      a7,21
   0x1428 <close+2>:    ecall
   0x142c <close+6>:    ret

(gdb) stepi

(gdb) x/3i 0x141e
=> 0x141e <write>:      li      a7,16
   0x1420 <write+2>:    ecall
   0x1424 <write+6>:    ret


# 现在 GDB 在 ecall 前停下（用户态）, 1420     # address of ecall in write
(gdb) p/x $pc
$2 = 0x1420


# stvec point to trampoline page,which call ecall, which will jump to uservec
# which is set by usertrapret
(gdb) p/x $stvec
$1 = 0x3ffffff000

# TODO : can not use stepi into the trampline
# must set a breakpoint at  0x3ffffff000 and set riscv use-compressed-breakpoints yes in .gdbinit
# set a breakpoint at the trampoline page address
(gdb) b *$stvec
# (gdb) b *0x3ffffff000    # Common xv6 trampoline virtual address
Breakpoint 2 at 0x3ffffff000



```



### 1. Trampoline entry: uservec(kernel/trampoline.S)

The code for the `uservec` trap handler is in trampoline.S (kernel/trampoline.S:16). 
When uservec starts, all 32 registers contain values owned by the interrupted user code. These 32 values need to be saved somewhere in memory, so that they can be restored when the trap returns to user space. Storing to memory requires use of a register to hold the address, but at this point there are no general-purpose registers available! 
Luckily RISC-V provides a helping hand in the form of the `sscratch` register. 

trap的最开始，CPU的所有状态都设置成运行用户代码而不是内核代码。在trap处理的过程中，我们实际上需要更改一些这里的状态，或者对状态做一些操作。这样我们才可以运行系统内核中普通的C程序。

kernel/trampoline.S uservec 汇编代码

On trap entry (`uservec` in trampoline.S):
1. Save user registers into the process’s `trapframe`.
2. Switches to kernel stack so we can run C code
3. Switch `satp` from the user’s page table to the kernel page table.
4. Jump into the kernal trap handler (`usertrap()` in trap.c).




#### A. save 32 user registers to the process’s trampframe
   struct trampframe in kernel/proc.h
   填充 struct tramframe(proc.h),利用 sscratch register 保存所有寄存器到 tramframe
   我们需要保存32个用户寄存器。
   因为很显然我们需要恢复用户应用程序的执行，尤其是当用户程序随机的被设备中断所打断时。我们希望内核能够响应中断，之后在用户程序完全无感知的情况下再恢复用户代码的执行。所以这意味着32个用户寄存器不能被内核弄乱。但是这些寄存器又要被内核代码所使用，所以在trap之前，你必须先在某处保存这32个用户寄存器。

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


```asm
csrrw a0, sscratch, a0
```
Now the user code’s a0 is saved in `sscratch`; uservec has one register (a0) to play with; and a0 contains the value the kernel previously placed in `sscratch`.

```bash
// In trampoline.S
.globl uservec
uservec:    
	#
  # trap.c sets stvec to point here, so
  # traps from user space start here,
  # in supervisor mode, but with a
  # user page table.
  #
  # sscratch points to where the process's p->trapframe is
  # mapped into user space, at TRAPFRAME.
  #
        
	# swap a0 and sscratch
  # so that a0 is TRAPFRAME
  csrrw a0, sscratch, a0
  # save the user registers in TRAPFRAME
  sd ra, 40(a0)
  sd sp, 48(a0)
  sd gp, 56(a0)
  sd tp, 64(a0)
  sd t0, 72(a0)
  sd t1, 80(a0)
  sd t2, 88(a0)
  sd s0, 96(a0)
  .....

  csrw sscratch, a0        # Save user a0
  ld sp, 8(a0)             # Load kernel stack pointer

  # Now using kernel stack for execution

```





```bash


# 单条指令执行 ecall，让 CPU 进入 trap（trampoline 将被映射并执行）：
# stepi 执行 ecall，QEMU 会触发 trap，trampoline 页被映射到地址空间
# xv6 的 trampoline/trap 入口通常有符号名/ 地址：
# trampoline 是用户态 → 内核态的入口。要 hit 它，必须有一个用户进程执行 ecall。
(gdb) stepi
Breakpoint 2, 0x0000003ffffff000 in ?? ()
=> 0x0000003ffffff000:  73 15 05 14     csrrw   a0,sscratch,a0




# ecall just the address of stvec regester point to : the begging of trampoline page
(gdb) p/x $stvec
$2 = 0x3ffffff000

# When a trap occurs, RISC-V saves the program counter to sepc register.
(gdb) p/x $sepc
$6 = 0xdfc

(gdb) p/x $satp
$12 = 0x0


# 此时你可以用 x/20i $pc、info registers、继续 stepi 来逐条看 trampoline / 用户 trap handler 的指令。
# we are the start of the trampoline page
(gdb) p/x $pc
$2 = 0x3ffffff000

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

(gdb) p/x $a0
$11 = 0x2

# sscratch point to the address of trampframe
(gdb) p/x $sscratch
$1 = 0x3fffffe000

# The `csrrw` instruction at the start of `uservec` swaps the contents of `a0` and `sscratch`. 
# sscratch points to where the process's p->trapframe

(gdb) si

# after swap, a0 with $sscratch register points to the trapframe
(gdb) p/x $a0
$10 = 0x3fffffe000

# sscratch point save the register a0
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


```

<!-- trapframe in kernel/proc.h -->
```c

struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;            // saved user ra (return address)
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};


(gdb) where
#0  0x0000003ffffff004 in ?? ()
// │B+ 0x3ffffff000        csrrw       a0,sscratch,a0                                         │
// │  >0x3ffffff004        sd  ra,40(a0)

// sd  ra,40(a0):
// Store the return address(the value in register ra) to memory at trapframe’s ra slot of address (a0 + 40)
// saves user’s ra before kernel code runs
// sd = “store doubleword” (i.e., store a 64-bit value)
// ra = return address register (x1)
// a0 = base register (here, holds the pointer to the trapframe)
// 40(a0) = memory address a0 + 40 bytes, corresponds to trapframe->ra
// So offset 40 is indeed the saved user-space ra.

(gdb) p/x $a0
$21 = 0x3fffffe000

// Print the memory address being written: trapframe->ra
(gdb) p/x $a0+40
$22 = 0x3fffffe028

// Show what’s in memory there:
// (x/gx = examine one “giant” (8-byte) value in hex)
(gdb) x/gx $a0+40
0x3fffffe028:   0x000000000000030e


(gdb) p/x $ra
$23 = 0x2a8

(gdb) p $ra
$7 = (void (*)()) 0x2a8 <getcmd+38>

(gdb) x $ra
0x2a8 <getcmd+38>:      Cannot access memory at address 0x2a8

(gdb) si

// after the above instruction, the register ra is saved to the trapframe->ra slot
(gdb) x/gx $a0+40
0x3fffffe028:   0x00000000000002a8
(gdb) x $a0+40
   0x3fffffe028:        addi    a0,sp,32

```

#### B. Switches to kernel stack so we can run C code 

```bash
  # save the user registers in TRAPFRAME
  sd ra, 40(a0)
  sd sp, 48(a0)
  ....

  csrw sscratch, a0        # Save user a0
  ld sp, 8(a0)             # Load kernel stack pointer



```
- a0 at this time holds the address of p->trapframe.
- p->trapframe->kernel_sp was set in scheduler() or allocproc() to point to the top of the process’s kernel stack.

- immediately after that instruction, the stack pointer (sp) now points to the process’s kernel stack.
- The CPU is still using the user page table (satp = user page table)
- But sp points into a kernel address, which is mapped in both user and kernel tables
(this is how the trap code runs safely — both page tables share trampoline and kernel stack mapping).

So from this point forward:
Any instruction that does push, pop, or uses the stack (e.g. sd ra, -8(sp)),
will write to the kernel stack memory region (not the user stack).


Switches to kernel stack 切换到 内核栈（相当于切换到进程对应的内核线程）
我们需要将堆栈寄存器指向位于内核的一个地址，因为我们需要一个堆栈来调用内核的C函数

Why do we “restore” it here?
When a trap happens:
1. The CPU is still using the user page table (user memory mappings).
2. The CPU switches into supervisor mode, but does not automatically switch stacks.
3. So xv6’s trampoline.S:uservec must manually switch to the process’s kernel stack before doing any kernel C code.

after ld sp, 8(a0)  # now SP points to process’s kernel stack, From now on, all kernel code (like usertrap(), syscall(), etc.) uses this stack

4. Then it jumps to usertrap() in trap.c, now executing on the kernel stack.



```c
(gdb) where
#0  0x0000003ffffff072 in ?? ()

(gdb) p/x $sp
$11 = 0x3fa0
(gdb) x $sp
0x3fa0: Cannot access memory at address 0x3fa0



// Each process in xv6 has its kernel stack allocated in procinit():
char *pa = kalloc();
uint64 va = KSTACK((int)(p - proc));  // e.g. 0x3fffffc000
p->kstack = va;

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)


// #  KSTACK: get the address of the kernel stack, for the first user process,p-proc=0? so 
(gdb) p/x $TRAMPOLINE - 2*$PGSIZE
(gdb) p/x $TRAMPOLINE - 6*$PGSIZE
$16 = 0x3fffffd000
(gdb) set $kstack=$TRAMPOLINE - 2*$PGSIZE
(gdb) p/x $kstack
$25 = 0x3fffffd000

// why?
(gdb) p/x p->kstack
$23 = 0x3fffffb000

// # Each kernel stack is one page (PGSIZE = 4096), and xv6 maps it like:
// # [kstack, kstack + PGSIZE)  ← valid kernel stack region

// # in trampoline.S
// # restore kernel stack pointer from p->trapframe->kernel_sp

// # This field: p->trapframe->kernel_sp  is set in usertrapret() (in kernel/trap.c):
p->trapframe->kernel_sp = p->kstack + PGSIZE;

// p->trapframe->kernel_sp 
(gdb) p/x $kstack+$PGSIZE
$18 = 0x3fffffe000


ld sp, 8(a0)  # now SP points to process’s kernel stack
// # make tp hold the current hartid, from p->trapframe->kernel_hartid
ld tp, 32(a0)
// # load the address of usertrap(), p->trapframe->kernel_trap
ld t0, 16(a0)



(gdb) si
0x0000003ffffff076 in ?? ()
=> 0x0000003ffffff076:  03 31 85 00     ld      sp,8(a0)


// # print the memory address being written: $a0+8 = trapframe->kernel_sp
(gdb) p/x $a0+8
$26 = 0x3fffffe008

// # load the address of the process’s kernel stack
(gdb) x/gx $a0+8
0x3fffffe008:   0x0000003fffffc000

// # print the instructions at the address of the process’s kernel stack
(gdb) x/6i $a0+8
   0x3fffffe008:        sw      s0,0(s0)
   0x3fffffe00a:        0xffff
   0x3fffffe00c:        0x80003c840000003f
   0x3fffffe014:        unimp
   0x3fffffe016:        unimp
   0x3fffffe018:        addi    a1,sp,552

// # before load the kernel stack pointer, we can see the stack pointer is user stack pointer
(gdb) p/x $sp
$27 = 0x3fa0

(gdb) x/gx $sp
0x3fa0: Cannot access memory at address 0x3fa0

(gdb) si
0x0000003ffffff07a in ?? ()
=> 0x0000003ffffff07a:  03 32 05 02     ld      tp,32(a0)

// #  now sp points to the kenel stack 
(gdb) p/x $sp
$21 = 0x3fffffc000



```

#### C. CPU switches from user page table to kernel page table 

切换到内核的地址空间

SATP寄存器现在正指向user page table，而user page table只包含了用户程序所需要的内存映射和一两个其他的映射，它并没有包含整个内核数据的内存映射。所以在运行内核代码之前，我们需要将SATP指向kernel page table
- 修改 satp register, 将SATP指向kernel page table
- sfence.vma

```bash
# restore kernel page table from p->trapframe->kernel_satp
ld t1, 0(a0)
csrw satp, t1
sfence.vma zero, zero


# when getinto the uservec, gdb can not get the content at $a1
(gdb) x/2c $a1
0x1d78: Cannot access memory at address 0x1d78

# before load the kernel page table, we can see the page table is user page table
(qemu) info mem
vaddr            paddr            size             attr
---------------- ---------------- ---------------- -------
0000000000000000 0000000087f60000 0000000000001000 rwxu-a-
0000000000001000 0000000087f5d000 0000000000001000 rwxu-a-
0000000000002000 0000000087f5c000 0000000000001000 rwx----
0000000000003000 0000000087f5b000 0000000000001000 rwxu-ad
0000003fffffe000 0000000087f6f000 0000000000001000 rw---ad
0000003ffffff000 000000008000a000 0000000000001000 r-x--a-


# load the kernel page table and flush the TLB
(gdb) si
0x0000003ffffff08a in ?? ()
=> 0x0000003ffffff08a:  73 00 00 12     sfence.vma

(gdb) p/x $satp
$35 = 0x8000000000087fff



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

#  in kenel/memlayout.h
// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L


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



```

#### D. 跳转到 tramframe->kernel_trap
   Jump into the kernal trap handler (`usertrap()` in trap.c)
  痛苦时间解除，跳转到 c 代码
  一旦我们设置好了，并且所有的硬件状态都适合在内核中使用， 我们需要跳入内核的C代码。

The CPU is now:
- using the kernel stack (sp)
- in supervisor mode
- running usertrap() in kernel address space

Inside usertrap(), the kernel will:
- Figure out what kind of trap happened (syscall, timer interrupt, etc.)
- Possibly copy data between user and kernel memory
- Handle scheduling, file I/O, etc.
- Then return to user mode later through usertrapret() and trampoline.S:userret

```bash
  # load the address of usertrap(), p->trapframe->kernel_trap
  ld t0, 16(a0)
  ...

  # jump to usertrap(), which does not return
  jr t0


(gdb) si
0x0000003ffffff07e in ?? ()
=> 0x0000003ffffff07e:  83 32 05 01     ld      t0,16(a0)
(gdb) si

(gdb) p/x $t0
$28 = 0x800029ee

# t0 points to usertrap now
(gdb) x $t0
   0x80003c84 <usertrap>:       addi    sp,sp,-48
(gdb) x/6i $t0
   0x80003c84 <usertrap>:       addi    sp,sp,-48
   0x80003c86 <usertrap+2>:     sd      ra,40(sp)
   0x80003c88 <usertrap+4>:     sd      s0,32(sp)
   0x80003c8a <usertrap+6>:     sd      s1,24(sp)
   0x80003c8c <usertrap+8>:     addi    s0,sp,48
   0x80003c8e <usertrap+10>:    sw      zero,-36(s0)



# # jump to usertrap(), which does not return
# jr t0
(gdb) si
usertrap () at kernel/trap.c:38

(gdb) x/4i $pc
=> 0x800029ee <usertrap>:       addi    sp,sp,-32
   0x800029f0 <usertrap+2>:     sd      ra,24(sp)
   0x800029f2 <usertrap+4>:     sd      s0,16(sp)
   0x800029f4 <usertrap+6>:     sd      s1,8(sp)

```





### 2. usertrap(kernel/trap.c)

The job of `usertrap` is to determine the cause of the trap, process it, and return (kernel/- trap.c:37). 

- It first changes `stvec` so that a trap while in the kernel will be handled by `kernelvec` rather than `uservec`. 
- It saves the `sepc` register (the saved user program counter), because usertrap might call yield to switch to another process’s kernel thread, and that process might return to user space, in the process of which it will modify sepc. 
  If the trap is a system call, usertrap calls syscall to handle it; 
  if a device interrupt, devintr; otherwise it’s an exception, and the kernel kills the faulting process. 
  The system call path adds four to the saved user program counter because RISC-V, in the case of a system call, leaves the program pointer pointing to the `ecall instruction` but user code needs to resume executing at the subsequent instruction.

- On the way out, usertrap checks if the process has been killed or should yield the CPU (if this trap is a timer interrupt).


```c
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}




(gdb) b usetrap


(gdb) c
Continuing.
Breakpoint 3, usertrap () at kernel/trap.c:38
38      {

(gdb) layout src

# 1=Supervisor, 0=User
(gdb) set $SSTAUS=1L<<8
(gdb) p/t $SSTAUS

// w_stvec((uint64)kernelvec);
// It switches the trap entry from uservec → kernelvec.
(gdb) p kernelvec
$39 = {<text variable, no debug info>} 0x80008a40 <kernelvec>
(gdb) x/10i *kernelvec
   0x80008a40 <kernelvec>:      addi    sp,sp,-256
   0x80008a42 <kernelvec+2>:    sd      ra,0(sp)
   0x80008a44 <kernelvec+4>:    sd      sp,8(sp)
   0x80008a46 <kernelvec+6>:    sd      gp,16(sp)
   0x80008a48 <kernelvec+8>:    sd      tp,24(sp)
   0x80008a4a <kernelvec+10>:   sd      t0,32(sp)
   0x80008a4c <kernelvec+12>:   sd      t1,40(sp)
   0x80008a4e <kernelvec+14>:   sd      t2,48(sp)
   0x80008a50 <kernelvec+16>:   sd      s0,56(sp)
   0x80008a52 <kernelvec+18>:   sd      s1,64(sp)

// 1. When a trap happens in user mode, the CPU:
// - Switches to supervisor mode.
// - Jumps to the address stored in stvec.
// At that moment, stvec points to: TRAMPOLINE + (uservec - trampoline)
// (because usertrapret() set it before returning to user mode).
// So the CPU jumps into uservec (in trampoline.S) — the entry point for user → kernel traps.

// 2. once we’re in the kernel now…
// When we’re executing in the kernel (usertrap()), we need to handle future traps differently:
// While we’re running kernel code, traps (like timer interrupts or page faults) should not go back to uservec.
// They should go to a kernel trap handler called kernelvec.



// // save user program counter.
// p->trapframe->epc = r_sepc();
(gdb) p/x $sepc
$40 = 0x1420

// the sepc register points to the instruction that caused the trap.
(gdb) x $sepc
0x1420 <write+2>:       Cannot access memory at address 0x1420

(gdb) p/x p->trapframe->epc
$41 = 0x1420

(gdb) p/x *(struct trapframe*)p->trapframe
$44 = {
  kernel_satp = 0x8000000000087fff,
  kernel_sp = 0x3fffffc000,
  kernel_trap = 0x80003c84,
  epc = 0x1420,
  kernel_hartid = 0x0,
  ra = 0x2a8,
  sp = 0x3fa0,
  gp = 0x505050505050505,
  tp = 0x505050505050505,
  t0 = 0x505050505050505,
  t1 = 0x505050505050505,
  t2 = 0x505050505050505,
  s0 = 0x3fc0,
  s1 = 0x505050505050505,
  a0 = 0x2,
  a1 = 0x1d78,
  a2 = 0x2,
  a3 = 0x505050505050505,
  a4 = 0x3,
  a5 = 0x64,
  a6 = 0x505050505050505,
  a7 = 0x10,
  s2 = 0x505050505050505,
  s3 = 0x505050505050505,
  s4 = 0x505050505050505,
  s5 = 0x505050505050505,
  s6 = 0x505050505050505,
  s7 = 0x505050505050505,
  s8 = 0x505050505050505,
  s9 = 0x505050505050505,
  s10 = 0x505050505050505,
  s11 = 0x505050505050505,
  t3 = 0x505050505050505,
  t4 = 0x505050505050505,
  t5 = 0x505050505050505,
  t6 = 0x505050505050505
}

// after p->trapframe->epc += 4;
(gdb) p/x p->trapframe->epc
$21 = 0x1424

// now the trapframe->epc is pointing to the next instruction after the ecall
(gdb) x/5i p->trapframe->epc
   0x1424 <write+6>:    Cannot access memory at address 0x1424


(gdb) p/x p->trapframe->kernel_sp
$21 = 0x3fffffc000
// sp is a bit lower than 0x3fffffc000 because it has pushed the trapframe data and the C call frames.
(gdb) p/x $sp
$22 = 0x3fffffbfd0


(gdb) x/5i $sp
   0x3fffffbfd0:        lw      a2,28(a3)
   0x3fffffbfd2:        c.srli64        s0
   0x3fffffbfd4:        unimp
   0x3fffffbfd6:        unimp
   0x3fffffbfd8:        unimp


// syscall();


```


### 3. Debug syscall (kenel/syscall.c)

```bash
  # num = p->trapframe->a7;   // syscall number from user
  # if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
  #   p->trapframe->a0 = syscalls[num](); // return value in a0

# retrival the syscall number from the trampframe
# SYS_write num is  16
(gdb) p num
$3 = 16

(gdb) print p->trapframe->a0
$5 = 2
(gdb) x/2c p->trapframe->a1
0x1d78: <error: Cannot access memory at address 0x1d78>

(gdb) print p->trapframe->a2
$7 = 1

```



#### 3.1. Debug sys_write(kernel/sysfile.c): How system call arguments flow from **user space → kernel space**.

That `if` line is checking that the user-supplied arguments (`fd`, `buf`, `n`) are valid before performing the actual write.
It extracts these values from the **saved user registers** inside the process’s trapframe, using helper functions that interpret each argument’s type and position correctly.

```c
uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  // This line checks and extracts the system call arguments that the user program passed to write(fd, buf, n)
  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}
```

How system call arguments flow from **user space → kernel space**.

When you call in user space:

```c
write(fd, buf, n);
```

1. The **user-space stub** (`usys.S`) loads:

   * `a0 = fd`
   * `a1 = buf`
   * `a2 = n`
   * `a7 = SYS_write`  ← syscall number for `write`

2. It executes:

   ```asm
   ecall
   ```

   → Triggers a **trap** into the kernel.

3. The CPU jumps to `uservec` → `usertrap()` → `syscall()`.

4. Inside `syscall()`:

   ```c
   num = p->trapframe->a7;
   p->trapframe->a0 = syscalls[num]();
   ```

   It calls `sys_write()` (the kernel’s handler).

---

Now inside `sys_write`

At this point:

* The user’s register values are stored in the process’s `trapframe`:

  ```
  a0 = fd
  a1 = buf (user virtual address)
  a2 = n
  ```

So `sys_write()` must *read these values* out of the trapframe.

That’s exactly what these helper functions do:

* `argfd(0, 0, &f)` → get **fd (file descriptor)** (1st argument)
* `argaddr(1, &p)` → get **p (user address of buffer)** (2nd argument)
* `argint(2, &n)` → get **n (count)** (3rd argument)

---

Breakdown of the condition

```c
if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
  return -1;
```

| Function          | Argument position | Purpose                                                                | Return value            |
| ----------------- | ----------------- | ---------------------------------------------------------------------- | ----------------------- |
| `argfd(0, 0, &f)` | 0th               | Get `fd` from user and check that it refers to a valid `struct file *` | `<0` if invalid fd      |
| `argaddr(1, &p)`  | 1st               | Get `p`, a user-space pointer to data buffer                           | `<0` if invalid address |
| `argint(2, &n)`   | 2nd               | Get integer `n`, the byte count                                        | `<0` if invalid         |

If any of these fails (e.g. user gave a bad pointer or fd), `sys_write()` immediately returns `-1`.

Otherwise, we continue to:

```c
return filewrite(f, p, n);
```

---

How `argfd`, `argint`, and `argaddr` work

They all rely on `argraw(n)`:

```c
static uint64 argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0: return p->trapframe->a0;
  case 1: return p->trapframe->a1;
  case 2: return p->trapframe->a2;
  ...
  }
}
```

So they extract the n-th argument from the saved registers.

Then:

* `argint(n, &i)` → reads `argraw(n)` as an integer.
* `argaddr(n, &addr)` → reads `argraw(n)` as a user-space address.
* `argfd(n, &fd, &f)` → uses the integer `fd` to look up a `struct file *` in the process’s open file table.

---

Flow summary for `write(fd, buf, n)`

```
user space: write(fd, buf, n)
    ↓
usys.S:
    a0 = fd, a1 = buf, a2 = n, a7 = SYS_write
    ecall
    ↓
kernel:
    usertrap() → syscall()
        ↓
    sys_write():
        argfd(0) → f
        argaddr(1) → p
        argint(2) → n
        filewrite(f, p, n)
```

---

Quick visual summary

| Stage                    | Register                     | Meaning             |
| ------------------------ | ---------------------------- | ------------------- |
| user `write(fd, buf, n)` | a0                           | fd                  |
|                          | a1                           | user buffer address |
|                          | a2                           | byte count          |
|                          | a7                           | syscall number      |
| kernel `sys_write`       | reads trapframe’s a0, a1, a2 | gets same values    |

---




```bash

(gdb) step
sys_write () at kernel/sysfile.c:91

(gdb) next
...

(gdb) p f
$3 = (struct file *) 0x80024e70 <ftable+24>

(gdb) p *(struct file *)f
$7 = {
  type = FD_DEVICE,
  ref = 6,
  readable = 1 '\001',
  writable = 1 '\001',
  pipe = 0x0 <runcmd>,
  ip = 0x80023308 <icache+160>,
  off = 0,
  major = 1
}

(gdb) p n
$7 = 2

(gdb) p/x p
$8 = 0x1d78

(gdb) x/s p
0x1d78: <error: Cannot access memory at address 0x1d78>


#  return to syscall
# >175             p->trapframe->a0 = syscalls[num](); // return value in a0  
(gdb) finish
Run till exit from #0  sys_write () at kernel/sysfile.c:91
0x0000000080002d64 in syscall () at kernel/syscall.c:175
175         p->trapframe->a0 = syscalls[num](); // return value in a0
Value returned is $8 = 1
 
```


#### 3.2 Debug filewrite()

“In sys_write(), the buffer pointer buf is a user virtual address, but the CPU is now in kernel mode, using the kernel page table — so how can the kernel actually read the user’s buffer contents?”
- The system call handler (sys_write) runs in kernel mode.
- Kernel mode normally uses the kernel page table (kernel_pagetable).
- The user buffer pointer p (from write(fd, buf, n)) is a user-space virtual address.
So if the kernel directly does:
memcpy(kernel_buf, (void *)p, n);

it would access memory using the kernel’s page table — which doesn’t map user space at low addresses like 0x0000....

That would cause a page fault

```c
// translate user virtual addresses → kernel-accessible physical addresses manually, using the user’s own page table (p->pagetable).
int copyin(void *dst, pagetable_t pagetable, uint64 srcva, uint64 len);

int copyout(pagetable_t pagetable, uint64 dstva, void *src, uint64 len);
```


```c
// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;

    // filewrite() → devsw[CONSOLE].write → consolewrite()
    // major = 1 means console
    // in console.c you’ll see:
    // void consoleinit(void)
    // {
    //   initlock(&cons.lock, "cons");
    //   devsw[CONSOLE].write = consolewrite;
    //   devsw[CONSOLE].read = consoleread;
    // }
    // So when you write to a device (e.g., stdout, which is the console):
    // Follow the function pointer, consolewrite(1, addr, n) 
   
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){

// The fd=1 corresponds to stdout, a console device (FD_DEVICE).
(gdb) p f->major
$10 = 1


```

#### 3.3 Debug consolewrite()
```c
(gdb) step
(gdb) where
#0  consolewrite (user_src=1, src=7544, n=2) at kernel/console.c:63
#1  0x0000000080006de2 in filewrite (f=0x80024e70 <ftable+24>, addr=7544, n=2)
    at kernel/file.c:147
#2  0x0000000080007baa in sys_write () at kernel/sysfile.c:94
#3  0x0000000080004402 in syscall () at kernel/syscall.c:175
#4  0x0000000080003d26 in usertrap () at kernel/trap.c:67
#5  0x00000000000002a8 in getcmd (
    buf=<error reading variable: Cannot access memory at address 0x3fa8>,
    nbuf=<error reading variable: Cannot access memory at address 0x3fa4>) at user/sh.c:137
Backtrace stopped: previous frame inner to this frame (corrupt stack?)



// user write()s to the console go here.
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  acquire(&cons.lock);
  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;

    // uartputc() sends the bytes to the UART (serial output), which QEMU maps to your terminal.
    uartputc(c);
  }
  release(&cons.lock);

  return i;
}

// Argument	Meaning	Action
// user_src == 1	buffer is from user space	call copyin()
// user_src == 0	buffer is in kernel space	use memmove() directly
(gdb) p user_src
$14 = 1

(gdb) x src
0x1d78: Cannot access memory at address 0x1d78


```

#### 3.4 Debug either_copyin()

```c
// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// consolewrite() calls either_copyin(), which calls copyin() because the buffer is user space.
// copyin() walks the user’s page table and copies bytes from the user buffer into the kernel.
(gdb) p user_src
$16 = 1

// p->pagetable is the user page table of the process —
// the page table that maps user virtual addresses (UVAs) (like 0x0–0x3ffffff…) to physical pages.
// Even though xv6 runs in kernel mode right now (because of a trap or syscall),
// it still keeps a pointer to the process’s own user page table in p->pagetable.
(gdb) p p->pagetable
$17 = (pagetable_t) 0x87f63000
// Why does its address look like 0x87f63000 (so high)?
// That address (0x87f63000) is not a virtual address in the user space —
// it’s a kernel virtual address pointing to a page table structure (a set of page table pages in physical memory).

// xv6 stores the page table in physical memory
// Each page table is a tree of pages (for RISC-V, 3 levels of 4 KiB pages).
// When xv6 creates a new process (in proc.c → allocproc() and vm.c → uvmcreate()), it allocates a new page in physical memory to hold the root page table (a level-2 page).

p->pagetable = uvmcreate();
// Inside uvmcreate():
pagetable_t pagetable = (pagetable_t) kalloc();

// kalloc() returns a kernel virtual address that maps to a physical page used as the root page table.
// So the address 0x87f63000 means:

// “The root page of this process’s user pagetable lives at physical address X,
// and the kernel can reach it via its kernel mapping at virtual address 0x87f63000.”


(gdb) x src
0x1d78: Cannot access memory at address 0x1d78

(gdb) x dst
0x3fffffbf0b:   -128 '\200'
```

#### 3.3 Debug copyin()
```c
// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}


(gdb) p pagetable
$20 = (pagetable_t) 0x87f63000

// You’ll see 8 64-bit entries — each a PTE (page table entry).
(gdb) x/8gx 0x87f63000
0x87f63000:     0x0000000021fd7c01      0x0000000000000000
0x87f63010:     0x0000000000000000      0x0000000000000000
0x87f63020:     0x0000000000000000      0x0000000000000000
0x87f63030:     0x0000000000000000      0x0000000000000000


(gdb) p srcva
$28 = 7544
(gdb) p/x srcva
$29 = 0x1d78
(gdb) x srcva
0x1d78: Cannot access memory at address 0x1d78
(gdb) p/x va0
$30 = 0x1000
(gdb) p/x pa0
$31 = 0x87f5d000
(gdb) x/s pa0
0x87f5d000:     "\273\a\367@\201'>\205bd\005a\202\200yq", <incomplete sequence \364>

// pa0+(srcva-va0): get the physical address of srcva
// now we can see the value of pa0+srcva-va0 with kernel pagetable 
(gdb) x/s pa0+(srcva-va0)
0x87f5dd78:     "$ "

(gdb) p dst
$32 = 0x3fffffbf0b "\200"
//  after memmove
(gdb) p dst
$33 = 0x3fffffbf0b "$"
```

### 4. usertrap->usertrapret(kernel/trap.c)
The first step in returning to user space is the call to `usertrapret` (kernel/trap.c:90). This function sets up the RISC-V control registers to prepare for a future trap from user space. 
- This involves changing `stvec` to refer to `uservec`, preparing the trapframe fields that uservec relies on, 
- setting `sepc` to the previously saved user program counter. 
- At the end, usertrapret calls `userret` on the trampoline page that is mapped in both user and kernel page tables; 
  the reason is that assembly code in userret will **switch page tables**.
  usertrapret’s call to `userret` passes TRAPFRAME in a0 and a pointer to the process’s user page table in a1 (kernel/trampoline.S:88). 


```c

(gdb) b usertrapret


// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  // So when an ecall happens in user mode, the CPU jumps to that trampoline page with the stvec regist pointer to
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  // p->kstack is the base address of the kernel stack for this process.
  // PGSIZE (4096 bytes) means the top of the stack.
  // Stacks grow downward, so this means sp starts at the top of a 1-page (4 KB) kernel stack.
  // Each process in xv6 has its own kernel stack, so when that process traps into the kernel (via a system call, interrupt, or exception), the CPU needs to use that process’s kernel stack.


  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}



//  the stevec register points to the kernelvec, which contains the address of the kernel trap handler.
(gdb) x $stvec
0x80008a40 <kernelvec>: 0xe80ee40ae0067111

(gdb) x/5i $stvec
   0x80008a40 <kernelvec>:      addi    sp,sp,-256
   0x80008a42 <kernelvec+2>:    sd      ra,0(sp)
   0x80008a44 <kernelvec+4>:    sd      sp,8(sp)
   0x80008a46 <kernelvec+6>:    sd      gp,16(sp)
   0x80008a48 <kernelvec+8>:    sd      tp,24(sp)


//  What are trampoline, uservec, and userret?
//  They’re labels (symbols) in the assembly file kernel/trampoline.S.
// Both uservec and trampoline are kernel address in kernel virtual space (their offsets within the kernel image).
// trampoline is the ep ntry point of the trampoline page.
(gdb) p uservec
$7 = 0x8000a000 <uservec> "s\025\005\024#4\025\002#8%\002#<5\002#0E\004#4U\004#8e\004#<u\004 \361$\365,\375P\341T\345X\351\\\355#0\005\v#4\025\v#8%\v#<5\v#0E\r#4U\r#8e\r#<u\r#0\205\017#4\225\017#8\245\017#<\265\017#0\305\021#4\325\021#8\345\021#<\365\021\363\""



(gdb) x/10i uservec
   0x8000a000 <uservec>:        csrrw   a0,sscratch,a0
   0x8000a004 <uservec+4>:      sd      ra,40(a0)
   0x8000a008 <uservec+8>:      sd      sp,48(a0)
   0x8000a00c <uservec+12>:     sd      gp,56(a0)
   0x8000a010 <uservec+16>:     sd      tp,64(a0)
   0x8000a014 <uservec+20>:     sd      t0,72(a0)
   0x8000a018 <uservec+24>:     sd      t1,80(a0)
   0x8000a01c <uservec+28>:     sd      t2,88(a0)
   0x8000a020 <uservec+32>:     sd      s0,96(a0)
   0x8000a022 <uservec+34>:     sd      s1,104(a0)

(gdb) p/x trampoline
$25 = 0x8000a000 <uservec>

// What is (uservec - trampoline)?
// Subtracting them gives you the byte offset from the start of the trampoline page to the entry point of uservec.
// means uservec is 0 bytes into trampoline.S page
(gdb) p/x uservec - trampoline
$9 = 0x0


// it’s setting the trap vector base address to address.
w_stvec(TRAMPOLINE + (uservec - trampoline));

// What is TRAMPOLINE + (uservec - trampoline)?
// The trampoline page is mapped into user virtual memory at address TRAMPOLINE (0x3ffffff000).
// So to get the virtual address of uservec in user space, we add its offset inside the trampoline code to the base of where that code is mapped.
// So this points exactly to the user-space-visible entry of uservec.

// TRAMPOLINE: Virtual address in user space (0x3ffffff000)
(gdb) set $MAXVA=1L<<38
(gdb) p/x $MAXVA
$13 = 0x4000000000

(gdb) set $PGSIZE=4096
(gdb) p $PGSIZE
$5 = 4096
(gdb) p/x $MAXVA-$PGSIZE
$5 = 0x3ffffff000
(gdb) set $TRAMPOLINE=$MAXVA-$PGSIZE
(gdb) p/x $TRAMPOLINE
$15 = 0x3ffffff000

(gdb) p p->trapframe
$10 = (struct trapframe *) 0x87f76000






// set S Exception Program Counter to the saved user pc.
// w_sepc(p->trapframe->epc);

// p->trapframe->epc contains the new user entry point.
(gdb) p/x p->trapframe->epc
$22 = 0x0

(gdb) p/x *p->trapframe
$15 = {
  kernel_satp = 0x8000000000087fff,
  kernel_sp = 0x3fffffe000,
  kernel_trap = 0x80003c84,
  epc = 0x0,
  kernel_hartid = 0x0,
  ra = 0x505050505050505,
  sp = 0x1000,
  gp = 0x505050505050505,
  tp = 0x505050505050505,
  t0 = 0x505050505050505,
  t1 = 0x505050505050505,
  t2 = 0x505050505050505,
  s0 = 0x505050505050505,
  s1 = 0x505050505050505,
  a0 = 0x505050505050505,
  a1 = 0x505050505050505,
  a2 = 0x505050505050505,
  a3 = 0x505050505050505,
  a4 = 0x505050505050505,
  a5 = 0x505050505050505,
  a6 = 0x505050505050505,
  a7 = 0x505050505050505,
  s2 = 0x505050505050505,
  s3 = 0x505050505050505,
  s4 = 0x505050505050505,
  s5 = 0x505050505050505,
  s6 = 0x505050505050505,
  s7 = 0x505050505050505,
  s8 = 0x505050505050505,
  s9 = 0x505050505050505,
  s10 = 0x505050505050505,
  s11 = 0x505050505050505,
  t3 = 0x505050505050505,
  t4 = 0x505050505050505,
  t5 = 0x505050505050505,
  t6 = 0x505050505050505
}


(gdb) p usertrap
$37 = {void (void)} 0x80003c84 <usertrap>
(gdb) x/5i usertrap
   0x80003c84 <usertrap>:       addi    sp,sp,-48
   0x80003c86 <usertrap+2>:     sd      ra,40(sp)
   0x80003c88 <usertrap+4>:     sd      s0,32(sp)
   0x80003c8a <usertrap+6>:     sd      s1,24(sp)
   0x80003c8c <usertrap+8>:     addi    s0,sp,48


// tell trampoline.S the user page table to switch to.
// uint64 satp = MAKE_SATP(p->pagetable);

(gdb) p/x p->pagetable
$25 = 0x87f75000


//  the satp register point to user page table
(gdb) x/8gx $satp
// 0x8000000000087fff:     Cannot access memory at address 0x8000000000087fff
// The RISC-V satp register (Supervisor Address Translation and Protection) holds
| MODE (bits 63–60) | ASID (bits 59–44) | PPN (bits 43–0) |
// PPN (physical page number) gives the physical address of the root page table.
// The full physical address = PPN << 12.
// The top bit (MODE = 8) means “Sv39 paging enabled”.


// that’s not a virtual address in the kernel’s address space.
// It’s a bit field value containing:
// mode bits (0x8 << 60)
// and the physical page number (0x87fff).

// Why GDB says “Cannot access memory at address 0x8000000000087fff”
// Because:
// GDB tries to read memory at virtual address 0x8000000000087fff,
// but that’s not a valid virtual address in xv6’s current kernel page table.
// It’s actually a bit-packed control register, not a pointer you can dereference.
// That is, $satp ≠ address of the page table in kernel space.

// How to get the real physical address of the root page table
(gdb) p/x $satp << 1 >>1  & ((1 << 44) - 1)
$2 = 0x87fff       # PPN
(gdb) p/x 0x87fff << 12
$3 = 0x87fff000    # physical address of page table
// So the physical address of the root page table = 0x87fff000.

// How to access that page table in GDB
// The kernel maps all physical memory at KERNBASE (0x80000000) using a direct map.

// showing valid PTEs
(gdb) x/8gx  0x87fff000
0x87fff000:     0x0000000021fff801      0x0000000000000000
0x87fff010:     0x0000000021ffe401      0x0000000000000000
0x87fff020:     0x0000000000000000      0x0000000000000000
0x87fff030:     0x0000000000000000      0x0000000000000000

(gdb) x/5i 0x87fff000
   0x87fff000:  bnez    s0,0x87ffef10
   0x87fff002:  0x21ff
   0x87fff004:  unimp
   0x87fff006:  unimp
   0x87fff008:  unimp


// // set S Exception Program Counter to the saved user pc.
// w_sepc(p->trapframe->epc);

// then jump to the user entry point at sepc
(gdb) p/x p->trapframe->epc
$46 = 0x1424
(gdb) x p->trapframe->epc
0x1424 <write+6>:       Cannot access memory at address 0x1424


// // tell trampoline.S the user page table to switch to.                   │
// uint64 satp = MAKE_SATP(p->pagetable); 
// Each process has its own pagetable. When xv6 switches into user mode, it writes that process’s page table root into satp
(gdb) p p->pagetable
$47 = (pagetable_t) 0x87f63000
(gdb) x p->pagetable
0x87f63000:     0x0000000021fd7c01

(gdb) p/x $satp
$2 = 0x8000000000087f63

//  fn point ot userret in trampoline.S
(gdb) p/x fn
$49 = 0x3ffffff090

(gdb) x/5i fn
   0x3ffffff090:        csrw    satp,a1
   0x3ffffff094:        sfence.vma
   0x3ffffff098:        ld      t0,112(a0)
   0x3ffffff09c:        csrw    sscratch,t0
   0x3ffffff0a0:        ld      ra,40(a0

(gdb) b *fn
Breakpoint 4 at 0x3ffffff090
(gdb) c
Continuing.

Breakpoint 4, 0x0000003ffffff090 in ?? ()
=> 0x0000003ffffff090:  73 90 05 18     csrw    satp,a1
(gdb) layout asm


```




### 5. trampoline Return: userret(kernel/trampoline.S)

- `userret` switches `satp` to the process’s user page table. 
Recall that the user page table maps both the trampoline page and TRAPFRAME, but nothing else from the kernel. The fact that the trampoline page is mapped at the same virtual address in user and kernel page tables is what allows uservec to keep executing after changing satp.
- userret copies the trapframe’s saved user a0 to `sscratch` in preparation for a later swap with TRAPFRAME. 
  From this point on, the only data userret can use is the register contents and the content of the trapframe. 
- Next userret restores saved user registers from the trapframe, does a final swap of a0 and sscratch to restore the user a0 and save TRAPFRAME for the next trap, and executes `sret` to return to user space.


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


The assembly in trampoline.S does:
```bash
userret:
    # a0: TRAPFRAME, in user page table.
    # a1: user page table, for satp.

    # switch to the user page table.
    csrw satp, a1
    sfence.vma zero, zero

    # put the saved user a0 in sscratch, so we
    # can swap it with our a0 (TRAPFRAME) in the last step.
    ld t0, 112(a0)
    csrw sscratch, t0
    ....
    # restore user a0, and save TRAPFRAME in sscratch
    csrrw a0, sscratch, a0


    # restore all but a0 from TRAPFRAME
    ld ra, 40(a0)
    ld sp, 48(a0)
    ld gp, 56(a0)
    ld tp, 64(a0)
    ld t0, 72(a0)
    ld t1, 80(a0)
    ld t2, 88(a0)
    ld s0, 96(a0)


    ld a0, 0(a0)         # load TRAPFRAME

          
    # return to user mode and user pc.
    # usertrapret() set up sstatus and sepc.
    sret

    ...



(gdb) p/x $a0
$52 = 0x3fffffe000
# a1 points to user page table
(gdb) p/x $a1
$53 = 0x8000000000087f63
# satp points to the kenerl page table
(gdb) p/x $satp
$54 = 0x8000000000087fff

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
0000000080000000 0000000080000000 000000000000b000 r-x--a-
000000008000b000 000000008000b000 0000000000002000 rw---ad
000000008000d000 000000008000d000 0000000000007000 rw-----
0000000080014000 0000000080014000 0000000000011000 rw---ad
0000000080025000 0000000080025000 0000000000001000 rw-----
0000000080026000 0000000080026000 0000000000003000 rw---ad
0000000080029000 0000000080029000 0000000007f32000 rw-----
0000000087f5b000 0000000087f5b000 000000000005d000 rw---ad
0000000087fb8000 0000000087fb8000 0000000000001000 rw---a-
0000000087fb9000 0000000087fb9000 0000000000046000 rw-----
0000000087fff000 0000000087fff000 0000000000001000 rw---a-
0000003ffff7f000 0000000087f77000 000000000003e000 rw-----
0000003fffffb000 0000000087fb5000 0000000000002000 rw---ad
0000003ffffff000 000000008000a000 0000000000001000 r-x--a-

# after switch csrw satp, a1
# csrw satp, a1
# sfence.vma zero, zero
(gdb) p/x $satp
$58 = 0x8000000000087f63

(qemu) info mem
vaddr            paddr            size             attr
---------------- ---------------- ---------------- -------
0000000000000000 0000000087f60000 0000000000001000 rwxu-a-
0000000000001000 0000000087f5d000 0000000000001000 rwxu-a-
0000000000002000 0000000087f5c000 0000000000001000 rwx----
0000000000003000 0000000087f5b000 0000000000001000 rwxu-ad
0000003fffffe000 0000000087f6f000 0000000000001000 rw---ad
0000003ffffff000 000000008000a000 0000000000001000 r-x--a-

```
So the CPU switches from kernel mode → user mode, jumps to the program’s entry point (in init/echo ELF), and begins executing user/_init or user/_echo.
After sret, CPU enters user mode, running _init/echo at its start address (e.g., 0x0)





# Code: First user process - sh


In xv6, the first user program is `user/init.c`. This is a fundamental part of how Unix-like operating systems bootstrap their user-space environment.


The init program is a simple user-space program that serves as the ancestor of all other user processes. In xv6, it's defined in init.c and has a very specific role: it creates and manages the initial `shell` processes.


## How the first user program is started

The process of starting the first user program involves several steps:

1. **Kernel initialization in main()**: The xv6 kernel boots up, initializes hardware, sets up memory management, process management, etc.
but no user processes exist yet

2. **Creating the first process in userinit()**: In the kernel's `main()` function (in `main.c`), after all kernel subsystems are initialized, the kernel calls `userinit()`.

**`userinit()` function**: This kernel function (typically in `proc.c`) creates the very first user process. It:
  - Allocates a process control block (PCB)
  - Sets up the process's memory space
  - loads hardcoded initcode into its memory
  - Sets up the process's initial registers and stack
  - Marks the process as ready to run

3. **Process scheduling in scheduler()**: The kernel's scheduler eventually picks up this first process and starts executing it in user mode.

First context switch: Scheduler runs the first user process (initcode)
The scheduler code is typically located in the `scheduler()` function in `proc.c`. This is the main scheduling loop that runs on each CPU core.

4. initcode executes: Makes exec("/init") system call

5. Kernel loads /init: Filesystem is now available, loads real init program

6. Normal operation: /init runs and can create other processes normally


## initcode
This is a clever bootstrapping technique that solves a fundamental chicken-and-egg problem in operating system initialization.

In xv6, `initcode` is a small piece of assembly code that becomes the initial user-space process. It's compiled from `user/initcode.S` and embedded as raw bytes in the kernel (kernel/proc.c) as an array called `initcode[]`.


### The Chicken-and-Egg Problem

The kernel needs to start the first user process, but there's a circular dependency:

1. To run a user program from the filesystem (like `/init`), you need system calls like `exec()`
2. But system calls can only be made from user mode (user processes)
3. But to have a user process, you first need to load a program from the filesystem
4. But to load from the filesystem, you need system calls...
   
### Why Use `initcode`?

The `initcode` array breaks this cycle by providing a tiny, hardcoded user program that's embedded directly in the kernel binary. Here's why this approach works:

#### 1. **No filesystem dependency**
- `initcode` is compiled into the kernel itself as raw bytes
- No need to read from disk or filesystem during the critical first process creation
- The kernel can directly copy these bytes into the first process's memory

#### 2. **Minimal bridge program**
- The `initcode` program is extremely simple - it just calls `exec("/init")`
- It's written in assembly and compiled to the smallest possible size
- Its only job is to bootstrap the "real" init program

#### 3. **Clean transition**
- Once `initcode` executes `exec("/init")`, it gets completely replaced by the actual `/init` program
- The process ID remains the same (PID 1), but now it's running the proper init program from the filesystem
- After this point, all subsequent processes can be created normally using `fork()` and `exec()`

### Why Not Load `/init` Directly?

The kernel could theoretically load `/init` directly, but that would require:
- Filesystem drivers to be fully initialized
- Complex file loading code in the kernel
- Mixing user program loading logic with kernel initialization

The `initcode` approach keeps the kernel's job simple: just copy some bytes into memory and start execution. The actual program loading from filesystem happens in user mode via the `exec()` system call, which is cleaner and more consistent with how all other programs are loaded.

This is a classic example of using a small, simple bootstrap program to enable a more complex system.



### The `init` program's behavior

Once `init` starts running, it typically:
- Opens file descriptors 0, 1, and 2 (stdin, stdout, stderr) connected to the console
- Forks child processes to run shell instances
- Waits for child processes and restarts them if they exit
- Essentially acts as the parent of all user processes

The transition from kernel mode to user mode for the first time is a critical moment in the boot process - it marks the point where the system moves from pure kernel execution to having actual user programs running.

This design follows the traditional Unix model where `init` (process ID 1) is the root of the entire process tree in user space.


## Kernel Stack

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





## Highlevel workflow


---

### 1. Booting up xv6

1. QEMU loads the xv6 kernel (`kernel/kernel` ELF).
2. The kernel starts executing from `start()` in `kernel/entry.S`.
3. It sets up the stack, switches to C, and runs `main()` in `kernel/main.c`.
4. `main()` initializes devices, memory, processes, etc.
5. Finally it calls:

   ```c
   userinit();
   scheduler();
   ```

   * `userinit()` creates the **first user process**, which runs the program `init` (from `user/init.c`).

---

### 2. `init` process starts userland

`init` is a **user program**, started by the kernel.
It runs `exec("/init", argv)` to replace its code with the user-level `init` program.

Then inside `init.c`:

```c
if(fork() == 0)
  exec("sh", argv);
wait(0);
```

→ This creates and runs the first **shell (`sh`)** process.
So, now you are in **user mode**, running `main()` in `user/sh.c`.

---

### 3. The shell (`sh`) waits for commands

Inside `sh.c`, the shell’s `main()` loop does roughly this:

```c
while(getcmd(buf, sizeof(buf)) >= 0){
  if(fork() == 0)
    runcmd(parsecmd(buf));
  wait(0);
}
```



## Debug First user process - sh


- [Xv6 代码导读 (调试工具配置；调试系统调用执行) [南京大学2022操作系统-P18]](https://www.bilibili.com/video/BV1DY4y1a7YD?spm_id_from=333.788.videopod.sections&vd_source=b3d4057adb36b9b243dc8d7a6fc41295)
- https://jyywiki.cn/OS/2022/slides/18.slides.html#/

### Boot xv6
```bash
gdb-multiarch  kernel/kernel 
```

### Debug main()


```c
// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}

```
### Debug procinit()

```c
// initialize the proc table at boot time.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");

      // Allocate a page for the process's kernel stack.
      // Map it high in memory, followed by an invalid
      // guard page.
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      
      // every process has its own kernel stack.
      uint64 va = KSTACK((int) (p - proc));
      // it’s mapped in the global kernel page table (in the vanilla xv6):
      kvmmap(kernel_pagetable, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
      p->kstack = va;
  }
  kvminithart();
}

```


### Debug userinit()

```c
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


// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}


(gdb) b userinit
(gdb) c
Breakpoint 2, userinit () at kernel/proc.c:240
=> 0x0000000080002d14 <userinit+8>:     97 00 00 00     auipc   ra,0x0

//  p->pid = allocpid(); 
(gdb) p p
$2 = (struct proc *) 0x80014d58 <proc>

// the first process
(gdb) p p->pid
$1 = 1
(gdb) p p->pagetable
$3 = (pagetable_t) 0x87f75000

// Since initcode is embedded as binary data in the kernel, you can examine it like any other global variable:
(gdb) p initcode
$4 =   "\027\005\000\000\023\005E\002\227\005\000\000\223\205\065\002\223\bp\000s\000\000\000\223\b \000s\000\000\000\357\360\237\377/init\000\000$\000\000\000\000\000\000\000"


(gdb) x/20i initcode
   0x8000b8d8 <initcode>:       auipc   a0,0x0
   0x8000b8dc <initcode+4>:     addi    a0,a0,36
   0x8000b8e0 <initcode+8>:     auipc   a1,0x0
   0x8000b8e4 <initcode+12>:    addi    a1,a1,35
   0x8000b8e8 <initcode+16>:    li      a7,7
   0x8000b8ec <initcode+20>:    ecall
   0x8000b8f0 <initcode+24>:    li      a7,2
   0x8000b8f4 <initcode+28>:    ecall
   0x8000b8f8 <initcode+32>:    jal     ra,0x8000b8f0 <initcode+24>
   0x8000b8fc <initcode+36>:    0x696e692f
   0x8000b900 <initcode+40>:    addi    a3,sp,12
   0x8000b902 <initcode+42>:    fld     fs0,8(s0)
   0x8000b904 <initcode+44>:    unimp
   0x8000b906 <initcode+46>:    unimp
   0x8000b908 <initcode+48>:    unimp
   0x8000b90a <initcode+50>:    unimp
   0x8000b90c:  unimp
   0x8000b90e:  unimp
   0x8000b910 <states.1772>:    fsd     fa2,96(a3)
   0x8000b912 <states.1772+2>:  0x8000

// You can also examine it as hexadecimal byte
(gdb) x/20bx initcode
0x8000b8d8 <initcode>:  0x17    0x05    0x00    0x00    0x13    0x05    0x45    0x02
0x8000b8e0 <initcode+8>:        0x97    0x05    0x00    0x00    0x93    0x85    0x35    0x02
0x8000b8e8 <initcode+16>:       0x93    0x08    0x70    0x00

```

#### Debug allocproc
```c
// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  // Finding an Unused Process Slot
  // If found, jumps to found: label with lock still held
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  // Assigns a unique process ID to the new process
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  memset(&p->context, 0, sizeof(p->context));

  // Set up new context to start executing at forkret,
  // which returns to user space.
  // Sets return address (ra) to forkret function
  p->context.ra = (uint64)forkret;
  // Sets stack pointer (sp) to top of kernel stack (stack grows down)
  // Note: p->kstack was set in procinit() in original xv6
  p->context.sp = p->kstack + PGSIZE;

  return p;
}


//  p->pagetable = proc_pagetable(p); 
(gdb) p p->pagetable
$67 = (pagetable_t) 0x87f75000

// memset(&p->context, 0, sizeof(p->context));
(gdb) p sizeof(p->context)
$69 = 112

```

#### Debug uvminit with initcode

```c
// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}
```

### Debug scheduler() in proc.c

```c
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    // So in vanilla xv6, when “no process is running,” the CPU is technically idle inside scheduler() — it’s just looping and calling intr_on() repeatedly.
    intr_on();
    
    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;

        // swtch in swtch.S, Save current registers in old. Load from new.	
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;

        found = 1;
      }
      release(&p->lock);
    }
#if !defined (LAB_FS)
    if(found == 0) {
      intr_on();
      asm volatile("wfi");
    }
#else
    ;
#endif
  }
}

(gdb) p p->pid
$19 = 1

(gdb) p p->pagetable
$18 = (pagetable_t) 0x87f75000

// swtch(&c->context, &p->context);
(gdb) p/x c->context
$17 = {
  ra = 0x0,
  sp = 0x0,
  s0 = 0x0,
  s1 = 0x0,
  s2 = 0x0,
  s3 = 0x0,
  s4 = 0x0,
  s5 = 0x0,
  s6 = 0x0,
  s7 = 0x0,
  s8 = 0x0,
  s9 = 0x0,
  s10 = 0x0,
  s11 = 0x0
}

(gdb) p/x p->context
$20 = {
  ra = 0x800035b2,
  sp = 0x3fffffe000,
  s0 = 0x0,
  s1 = 0x0,
  s2 = 0x0,
  s3 = 0x0,
  s4 = 0x0,
  s5 = 0x0,
  s6 = 0x0,
  s7 = 0x0,
  s8 = 0x0,
  s9 = 0x0,
  s10 = 0x0,
  s11 = 0x0
}

// ra (Return Address) = 0x800035b2 <forkret> which is set in allocproc
// The ra register contains the return address - where execution should continue after the context switch.
// Value: 0x800035b2 points to the forkret function
// Meaning: When swtch() completes and returns, execution will jump to forkret
// Why forkret?: This is set up during process creation. The first time a process runs, it needs special initialization, which forkret handles
(gdb) x p->context->ra
0x800035b2 <forkret>:   0x41

// sp (Stack Pointer) = 0x3fffffe000
// The sp register points to the process's kernel stack.
// Value: 0x3fffffe000 is near the top of the kernel stack for this process
// Meaning: This is where the process's kernel-mode stack is located
// Important: This is the kernel stack, not the user stack (which is in trapframe->sp)

(gdb) x p->context->sp
0x3fffffe000:   Cannot access memory at address 0x3fffffe000

// When swtch(&c->context, &p->context) executes:
// 1. Save scheduler context: Current CPU registers are saved to c->context (which are currently mostly 0 since scheduler just started)
// 2. Load process context: Process registers are loaded from p->context:
// ra = 0x800035b2 (forkret)
// sp = 0x3fffffe000 (kernel stack)
// Other saved registers (s0-s11) are restored
// 3. Jump to ra: swtch() returns by jumping to the address in ra, which is forkret

```



#### Debug forkret

```c
// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    // Initializes the file system (only for the very first process)
    fsinit(ROOTDEV);
  }
  // Calls usertrapret() to transition to user mode
  usertrapret();
}

(gdb) p first
$22 = 1

```

Why This Design?
This two-stage approach (scheduler → forkret → user mode) allows:
- Clean separation: Scheduler handles process selection, forkret handles process initialization
- Consistent interface: All processes (including the first one) go through the same transition mechanism
- Proper locking: Process lock is properly managed during the transition

The Complete Flow
```
scheduler() finds RUNNABLE process
    ↓
swtch() switches to process context
    ↓ (ra points here)
forkret() - first-time process setup
    ↓
usertrapret() - transition to user mode
    ↓
initcode starts executing in user space
```

#### Debug usertrapret in trap.c



#### Debug userret in trampoline.S




###  Debug write (see 4.2.3 trap process with ecall)
ecall -> uservec -> usetrap ->syscall -> sys_write -> usertrapret -> userret









# 4.3 Code: Calling system calls

## system call processing

1. `initcode.S` places the `arguments` for exec in `registers a0 and a1`, and puts the `system call number in a7`. 
2. System call numbers match the entries in the syscalls array, a table of function pointers (kernel/syscall.c:108). 
3. The `ecall` instruction traps into the kernel and causes `uservec`, `usertrap`, and then syscall to execute, as we saw above.
4. `syscall` (kernel/syscall.c:133) retrieves the system call number from the saved a7 in the trapframe and uses it to index into syscalls. 
For the first system call, a7 contains `SYS_exec` (kernel/syscall.h:8), resulting in a call to the system call implementation function `sys_exec`.
5. When sys_exec returns, syscall records its return value in `p->trapframe->a0`. 
This will cause the original user-space call to `exec()` to return that value, since the C calling convention on RISC-V places return values in a0. 
System calls conventionally return negative numbers to indicate errors, and zero or positive numbers for success. If the system call number is invalid, syscall prints an error and returns -1.




## High-level worlflow of call echo

```css
User shell:
  main()
   └─> fork()    → new process (child)
        └─> exec("echo", argv)
              └─> sys_exec() → exec() (in kernel)
                     ├─ load ELF, set trapframe->epc = entry
                     └─ return to syscall() / usertrapret()
                          └─ jump to trampoline.S → sret → user mode
                               └─ pc = ELF entry (_start in echo)
                                    └─ call main(argc, argv)
```



## Debug a user-space program: echo


### 0. Boot xv6

```bash
# in qemu terminal
make clean && make qemu-gdb
QEMU 4.2.1 monitor - type 'help' for more information
(qemu) Gdk-Message: 23:11:40.757: Unable to load hand2 from the cursor theme

# // after xv6 is booted
# // xv6 kernel is booting
# // init: starting sh
$ echo hi



# in gdb terminal
gdb-multiarch -x .gdbinit_kernel  kernel/kernel 

# Kernel GDB session only knows kernel structs.
# To inspect cmd (a user-space struct cmd from sh.c), you must load the user/_sh ELF symbols with add-symbol-file.
# === 加载 kernel/kernel 符号（确保能看到 trampoline/usertrap）===
(gdb) add-symbol-file user/_sh 0x0
add symbol table from file "user/_sh" at
        .text_addr = 0x0
Reading symbols from user/_sh...
# Meaning: “Load the debug symbols for user/_sh, and tell GDB that the code starts at virtual address 0x0.”
# user/_sh is the ELF for the shell program (compiled with symbols).
# 0x0 is its load base (xv6 loads user programs at VA 0 by kernel)
# After this, GDB will know how to map struct cmd, main(), etc. to the addresses you see in your cmd pointer.


# This attaches GDB to QEMU
(gdb) target remote :26000
Remote debugging using :26000
0x0000000000001000 in ?? ()

```

### 1. Debug in user/sh.c

#### debug getcmd() in user/sh.c

```bash
# from user/sh.asm
# 0000000000000282 <getcmd>:
# }

# int
# getcmd(char *buf, int nbuf)
# {
#      282:	1101                	addi	sp,sp,-32
#      284:	ec06                	sd	ra,24(sp)
#      286:	e822                	sd	s0,16(sp)
#      288:	1000                	addi	s0,sp,32
#      28a:	fea43423          	sd	a0,-24(s0)
#      28e:	87ae                	mv	a5,a1
#      290:	fef42223          	sw	a5,-28(s0)
#   // fprintf(2, "$ ");
#   write(2, "$ ", 2); // use the code as vedio course 2020
#      294:	4609                	li	a2,2
#      296:	00002597          	auipc	a1,0x2
#      29a:	ae258593          	addi	a1,a1,-1310 # 1d78 <statistics+0x11c>
#      29e:	4509                	li	a0,2
#      2a0:	00001097          	auipc	ra,0x1
#      2a4:	17e080e7          	jalr	382(ra) # 141e <write>

#   memset(buf, 0, nbuf);
#      2a8:	fe442783          	lw	a5,-28(s0)
#      2ac:	863e                	mv	a2,a5
#      2ae:	4581                	li	a1,0
#      2b0:	fe843503          	ld	a0,-24(s0)
#      2b4:	00001097          	auipc	ra,0x1
#      2b8:	d90080e7          	jalr	-624(ra) # 1044 <memset>
#   // to read from stdin(file descriptor 0), which blocks and waits for user input.
#   gets(buf, nbuf); // 
#      2bc:	fe442783          	lw	a5,-28(s0)
#      2c0:	85be                	mv	a1,a5
#      2c2:	fe843503          	ld	a0,-24(s0)
#      2c6:	00001097          	auipc	ra,0x1
#      2ca:	e28080e7          	jalr	-472(ra) # 10ee <gets>
#   if(buf[0] == 0) // EOF, if the first character is 0 (meaning end-of-file), it returns -1 to stop the main loop.
#      2ce:	fe843783          	ld	a5,-24(s0)
#      2d2:	0007c783          	lbu	a5,0(a5)
#      2d6:	e399                	bnez	a5,2dc <getcmd+0x5a>
#     return -1;
#      2d8:	57fd                	li	a5,-1
#      2da:	a011                	j	2de <getcmd+0x5c>
#   return 0;
#      2dc:	4781                	li	a5,0
# }
#      2de:	853e                	mv	a0,a5
#      2e0:	60e2                	ld	ra,24(sp)
#      2e2:	6442                	ld	s0,16(sp)
#      2e4:	6105                	addi	sp,sp,32
#      2e6:	8082                	ret


# (gdb) b getcmd
# (gdb) b 0x282                  # Break at memset in shell's getcmd()
# Num     Type           Disp Enb Address            What
# 5       breakpoint     keep y   0x0000000000000282 in getcmd at user/sh.c:135
#         breakpoint already hit 2 times

# gets(buf, nbuf);  at line 135, the program will stop at this line waiting for user input.
# if(buf[0] == 0) 
#  set breakpoint at after get the buf from user input in getcmd()
(gdb) b *0x2ce
Breakpoint 8 at 0x2ce: file user/sh.c, line 142.
(gdb) info b
Num     Type           Disp Enb Address            What
8       breakpoint     keep y   0x00000000000002ce in getcmd at user/sh.c:142


# qemu ternimal 
$ echo hi



Breakpoint 8, getcmd (buf=0x1ec8 <buf> "echo hi\n", nbuf=100) at user/sh.c:142
142       if(buf[0] == 0) // EOF, if the first character is 0 (meaning end-of-file), it returns -1 to stop the main loop.
=> 0x00000000000002ce <getcmd+76>:      83 37 84 fe     ld      a5,-24(s0)
   0x00000000000002d2 <getcmd+80>:      83 c7 07 00     lbu     a5,0(a5)


# we are breaking after get the buffer from user input
(gdb) x $pc
0x2ce <getcmd+76>:      0x83


(gdb) x/6i $pc
=> 0x2ce <getcmd+76>:   ld      a5,-24(s0)
   0x2d2 <getcmd+80>:   lbu     a5,0(a5)
   0x2d6 <getcmd+84>:   bnez    a5,0x2dc <getcmd+90>
   0x2d8 <getcmd+86>:   li      a5,-1
   0x2da <getcmd+88>:   j       0x2de <getcmd+92>
   0x2dc <getcmd+90>:   li      a5,0

# when tui disable 
(gdb) list *$pc
0x2ce is in getcmd (user/sh.c:142).
137       write(2, "$ ", 2); // use the code as vedio course 2020
138
139       memset(buf, 0, nbuf);
140       // to read from stdin(file descriptor 0), which blocks and waits for user input.
141       gets(buf, nbuf); //
142       if(buf[0] == 0) // EOF, if the first character is 0 (meaning end-of-file), it returns -1 to stop the main loop.
143         return -1;
144       return 0;
145     }
146


(gdb)  where
#0  getcmd (buf=0x1ec8 <buf> "echo hi\n", nbuf=100) at user/sh.c:142
#1  0x000000000000041c in main () at user/sh.c:162
#2  0x00000000000000bc in runcmd (
    cmd=<error reading variable: Cannot access memory at address 0x2f98>)
    at user/sh.c:84
Backtrace stopped: previous frame inner to this frame (corrupt stack?)


(gdb) p buf
$13 = 0x1ec8 <buf> "echo hi\n"

(gdb) x/s buf
0x1ec8 <buf.1141>:      "echo hi\n"

(gdb) x/8c buf
0x1520 <buf.1141>:      101 'e' 99 'c'  104 'h' 111 'o' 32 ' '  104 'h' 105 'i'10 '\n'




(gdb) info frame
# Stack level 0, frame at 0x3fa0:
#  pc = 0x0 in getcmd (user/sh.c:135); saved pc = 0xadc
#  called by frame at 0x3fe0
#  source language c.
#  Arglist at 0x3fa0, args: buf=buf@entry=0x1520 <buf> "echo hi\n", nbuf=nbuf@entry=100
#  Locals at 0x3fa0, Previous frame's sp is 0x3fa0
# Could not fetch register "ustatus"; remote failure reply 'E14'

```


#### debug parsecmd() in user/sh.c

```bash
(gdb) b parsecmd
(gdb) c
Continuing.

Breakpoint 9, parsecmd (s=0x1ec8 <buf> "echo hi\n") at user/sh.c:336
=> 0x00000000000008da <parsecmd+14>:    83 34 84 fc     ld      s1,-56(s0)

(gdb) p cmd
$21 = (struct cmd *) 0x13f50
(gdb) x cmd
0x13f50:        "\001"

(gdb) p * (struct cmd *)cmd
$22 = {
  type = 1
}
```

#### debug runcmd() in user/sh.c

```bash
# set a breakpoint at runcmd()
(gdb) b runcmd
(gdb) c
Continuing.

Breakpoint 10, runcmd (cmd=0x13f50) at user/sh.c:67
=> 0x000000000000000c <runcmd+12>:      83 37 84 fb     ld      a5,-72(s0)
   0x0000000000000010 <runcmd+16>:      91 e7   bnez    a5,0x1c <runcmd+28>

#  the memory map is user space
(qemu) info mem
vaddr            paddr            size             attr
---------------- ---------------- ---------------- -------
0000000000000000 0000000087f70000 0000000000001000 rwxu-a-
0000000000001000 0000000087f66000 0000000000001000 rwxu-ad
0000000000002000 0000000087f71000 0000000000001000 rwx----
0000000000003000 0000000087f5a000 0000000000001000 rwxu-ad
0000000000004000 0000000087f59000 0000000000001000 rwxu-ad
0000000000005000 0000000087f58000 0000000000001000 rwxu---
0000000000006000 0000000087f57000 0000000000001000 rwxu---
0000000000007000 0000000087f56000 0000000000001000 rwxu---
0000000000008000 0000000087f55000 0000000000001000 rwxu---
0000000000009000 0000000087f54000 0000000000001000 rwxu---
000000000000a000 0000000087f53000 0000000000001000 rwxu---
000000000000b000 0000000087f52000 0000000000001000 rwxu---
000000000000c000 0000000087f51000 0000000000001000 rwxu---
000000000000d000 0000000087f50000 0000000000001000 rwxu---
000000000000e000 0000000087f4f000 0000000000001000 rwxu---
000000000000f000 0000000087f4e000 0000000000001000 rwxu---
0000000000010000 0000000087f4d000 0000000000001000 rwxu---
0000000000011000 0000000087f4c000 0000000000001000 rwxu---
0000000000012000 0000000087f4b000 0000000000001000 rwxu---
0000000000013000 0000000087f4a000 0000000000001000 rwxu-ad
0000003fffffe000 0000000087f64000 0000000000001000 rw---ad
0000003ffffff000 000000008000a000 0000000000001000 r-x--a-




# print the stack pointer
(gdb) info reg sp
sp             0x3f70   0x3f70

# If the current PC (info reg pc) points inside kernel text, you’re in kernel mode.
# address ≥ 0x80000000 → kernel virtual address → kernel mode
# If it points near 0x00000000 (user space), you’re in user mode.
(gdb) info reg pc
pc             0xc      0xc <runcmd+12>


# Extracting the SPP bit from sstatus register
# SPP (bit 8 in the 64-bit sstatus on RISC-V) tells you what mode the CPU will return to when executing sret.
# SPP = 0 → previous mode = U-mode
# SPP = 1 → previous mode = S-mode
(gdb) p/t $sstatus
$8 = 100010
(gdb) p ($sstatus >> 8) & 1
$9 = 0



# cmd is a pointer to a struct cmd
(gdb) p cmd
$2 = (struct cmd *) 0x13f50

(gdb) p *cmd
$12 = {
  type = 1
}

# case EXEC:
#   ecmd = (struct execcmd*)cmd;
#   if(ecmd->argv[0] == 0)
#     exit(1);
(gdb) p *((struct execcmd *)cmd)
$18 = {
  type = 1,
  argv =     {0x1ec8 <buf> "echo",
    0x1ecd <buf+5> "hi",
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>},
  eargv =     {0x1ecc <buf+4> "",
    0x1ecf <buf+7> "",
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>}
}

(gdb) p *ecmd
$15 = {
  type = 1,
  argv =     {0x1ec8 <buf> "echo",
    0x1ecd <buf+5> "hi",
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>},
  eargv =     {0x1ecc <buf+4> "",
    0x1ecf <buf+7> "",
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>,
    0x0 <runcmd>}
}



# exec(ecmd->argv[0], ecmd->argv);
# fprintf(2, "exec %s failed\n", ecmd->argv[0]);
# break;

(gdb) p ecmd->type
$30 = 1

(gdb) p ecmd->argv
$24 =   {0x1ec8 <buf> "echo",
  0x1ecd <buf+5> "hi",
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>}

(gdb) p ecmd->eargv
$25 =   {0x1ecc <buf+4> "",
  0x1ecf <buf+7> "",
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>}

(gdb) p ecmd->argv[0]
$32 = 0x1ec8 <buf> "echo"
(gdb) p ecmd->argv[1]
$33 = 0x1ecd <buf+5> "hi"
(gdb) p ecmd->argv[2]
$34 = 0x0 <runcmd>

(gdb) p ecmd->argv
$5 =   {0x1ec8 <buf> "echo",
  0x1ecd <buf+5> "hi",
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>,
  0x0 <runcmd>}

(gdb) x ecmd->argv
0x13f58:        0x00001ec8

# so when exec(ecmd->argv[0], ecmd->argv);, the kernel will call exec() with the arguments.

```

When you type:

```
echo hi
```

* The shell reads this line into `buf`.
* `parsecmd()` parses it into a command structure (a `struct cmd`).
* Then it **forks** a new process to execute the command.
* In the **child process** (after `fork()`):

```c
runcmd(parsecmd(buf));
// `runcmd()` (in `sh.c`) will eventually call:

exec("echo", argv);
// which triggers a **system call** to the kernel:

// Now the CPU is in **user mode**, running `main()` in `user/echo.c`:

int main(int argc, char *argv[]) {
  for(int i = 1; i < argc; i++)
    write(1, argv[i], strlen(argv[i]));
  write(1, "\n", 1);
  exit(0);
}
// This writes `hi` to stdout, then calls `exit()`.
```


### 2. Debug  ecall trap  (check 4.2.3 trap process from ecall)


#### 0. Debug ecall after exec()

after exec(path="echo", argv=["echo","hi"]), the system will call ecall 


```c
  // case EXEC:
  //   ecmd = (struct execcmd*)cmd;
  //   if(ecmd->argv[0] == 0)
  //     exit(1);
  //   exec(ecmd->argv[0], ecmd->argv);
  //   fprintf(2, "exec %s failed\n", ecmd->argv[0]);
  //   break;
(gdb) b exec
Breakpoint 4 at 0x1436: exec. (2 locations)
(gdb) c
Continuing.

Breakpoint 4, exec () at user/usys.S:55
=> 0x0000000000001436 <exec+0>: 9d 48   li      a7,7

(gdb) step
user/usys.S
.global exec
exec:
 li a7, SYS_exec
 ecall
 ret

// $a7 is the ecall number
(gdb) p $a7
$11 = 12

// $a0 is the first argument, which is the path
// $a1 is the second argument, which is the argv
(gdb) p/x $a0
$7 = 0x1ec8
(gdb) x/s $a0
0x1ec8 <buf.1141>:      "echo"

(gdb) p/x $a1
$8 = 0x13f58

// To see the actual command line arguments as strings, you can use:
(gdb) x/10s *$a1
// 0x1ec8 <buf.1141>:      "echo"
// 0x1ecd <buf.1141+5>:    "hi"
// 0x1ed0 <buf.1141+8>:    ""
// 0x1ed1 <buf.1141+9>:    ""
// 0x1ed2 <buf.1141+10>:   ""
// 0x1ed3 <buf.1141+11>:   ""
// 0x1ed4 <buf.1141+12>:   ""
// 0x1ed5 <buf.1141+13>:   ""
// 0x1ed6 <buf.1141+14>:   ""
// 0x1ed7 <buf.1141+15>:   "

// You can also examine individual arguments:
(gdb) x/s *($a1)
0x1ec8 <buf.1141>:      "echo"
(gdb) x/s *(($a1)+8)
0x1ecd <buf.1141+5>:    "hi"

// stvec register poiter to trampoline 
// the CPU will trap into supervisor mode (kernel mode) using the trampoline code at address TRAMPOLINE = 0x3ffffff000
(gdb) p/x $stvec
$2 = 0x3ffffff000 

(gdb) p/x *$stvec
Cannot access memory at address 0x3ffffff000

// (gdb) b kernel/trampoline.S:uservec
(gdb) b *$stvec
Breakpoint 4 at 0x3ffffff000

(gdb) si
Breakpoint 4, 0x0000003ffffff000 in ?? ()
=> 0x0000003ffffff000:  73 15 05 14     csrrw   a0,sscratch,a0
```


#### 1. Debug uservec

```bash

(gdb) where
#0  0x0000003ffffff000 in ?? ()

(gdb) p/x $pc
$4 = 0x3ffffff000

(gdb) p/x $sscratch
$6 = 0x3fffffe000

# when you get into the usevec, you can access memory at user address
(gdb) x/s $a0
0x1ec8 <buf.1141>:      <error: Cannot access memory at address 0x1ec8>

(gdb) x/10s *$a1
Cannot access memory at address 0x13f58
(gdb) x/s *($a1)
Cannot access memory at address 0x13f58

```
#### 2. Debug usertrap
```bash
(gdb) b usertrap

```
#### 3. Debug syscall of exec
```bash
(gdb) b syscall

#define SYS_exec    7
# num= 7 is exec syscall number
(gdb) p num
$16 = 7

```
##### Debug sys_exec() in sysfile.c

```c

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;
  // get the path from p->trapframe->a0
  // get the argv from p->trapframe->a1
  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;



(gdb) b sys_exec

(gdb) c
Continuing.

Breakpoint 5, sys_exec () at kernel/sysfile.c:425
=> 0x0000000080008726 <sys_exec+8>:     93 07 84 f6     addi    a5,s0,-152
   0x000000008000872a <sys_exec+12>:    13 06 00 08     li      a2,128

(gdb) p path
$20 =   "echo\000\000\000\000XI\001\200\000\000\000\000@\237\377\377?\000\000\000P\237\377\3
77?\000\000\000 \000\000\000\000\000\000\000`\237\377\377?\000\000\000\"\000\000\000\000\000
\000\000XI\001\200\000\000\000\000`\237\377\377?\000\000\000\200\237\377\377?\000\000\000\26
4\024\000\200\000\000\000\000XI\001\200\000\000\000\000XI\001\200\000\000\000\000\240\237\37
7\377?\000\000\000\316)\000\200\000\000\000\000HP\001\200\000\000\000"

(gdb) x/s path
0x3fffff9f08:   "echo"

(gdb) p/x uargv
$24 = 0x13f58


```
##### Debug fetchaddr

##### Debug copyin

##### Debug fetchstr

```c

(gdb) p argv
$26 =   {0x87f49000 "echo",
  0x87f48000 "hi",
  0x0 <runcmd> <repeats 30 times>}
```

##### Debug exec  (see chapter 3.8)

1. Loads the `echo` ELF file(e.g. /bin/echo) from disk into memory.
2. create a new user page table for the process.
3. Allocate a new user stack and copy argv[] strings arguments (`"hi"`) into user space.
4.  Sets the **user program counter (epc)** to the entry point of `echo`: trapframe->epc = ELF entry address (program’s _start).
5.  Set up trapframe->sp = top of the new user stack.
6. Free the old user page table.

At the end, xv6 still has the same struct proc (PID unchanged),
but it now represents a completely new user program (echo)


```c
(gdb) b exec
(gdb) c
Continuing.

Breakpoint 11, exec (path=0x3fffff9f08 "echo", argv=0x3fffff9e08)
    at kernel/exec.c:17
=> 0x00000000800073de <exec+26>:        23 3c 04 fa     sd      zero,-72(s0)

// Set a breakpoint at the return statement by line number:
(gdb) b exec.c:146


(gdb) p path
$14 = 0x3fffff9f08 "echo"

(gdb) p argv
$16 = (char **) 0x3fffff9e08

(gdb) p/s argv[0]
$17 = 0x87f49000 "echo"

(gdb) p/s argv[1]
$18 = 0x87f48000 "hi"

// struct proc *p = myproc(); // initializes a local pointer p to the current process.
(gdb) p p->pid
$19 = 3

// in  proc_pagetable(struct proc *p)
(gdb) info reg sp
sp             0x3fffff9ba0     0x3fffff9ba0

(gdb) info frame
// Stack level 0, frame at 0x3fffff9bd0:
//  pc = 0x80002c54 in proc_pagetable (kernel/proc.c:202); saved pc = 0x800074b8
//  called by frame at 0x3fffff9df0
//  source language c.
//  Arglist at 0x3fffff9bd0, args: p=0x80015048 <proc+752>
//  Locals at 0x3fffff9bd0, Previous frame's sp is 0x3fffff9bd0
//  Saved registers:
//   ra at 0x3fffff9bc8, fp at 0x3fffff9bc0, pc at 0x3fffff9bc8Could not fetch register "ustatus"; remote failure reply 'E14'

(gdb) info stack
// #0  0x0000000080002c54 in proc_pagetable (p=0x80015048 <proc+752>) at kernel/proc.c:202
// #1  0x00000000800074b8 in exec (path=0x3fffff9f08 "echo", argv=0x3fffff9e08) at kernel/exec.c:41
// #2  0x0000000080008838 in sys_exec () at kernel/sysfile.c:447
// #3  0x0000000080004402 in syscall () at kernel/syscall.c:175
// #4  0x0000000080003d26 in usertrap () at kernel/trap.c:67
// #5  0x000000000000008c in runcmd (cmd=<error reading variable: Cannot access memory at address 0x3f78>) at user/sh.c:78
// Backtrace stopped: previous frame inner to this frame (corrupt stack?)


```

##### Debug exec -> sys_exec -> syscall ->usetrap

After exec() returns — still inside kernel mode
exec() (in sys_exec()) eventually returns 0 to the syscall handler.
At this point:
- The process is still in kernel mode (we haven’t yet returned to user space).
- The kernel’s syscall handler (syscall()) will finish and return to usertrapret()


```c

// in sys_exec(void) in kernel/sysfile.c
int ret = exec(path, argv);
```

-> usertrapret

##### Debug  usetrapret -> useret

usertrapret() prepares to return to new user space
```c
// in exec() kernel/exec.c 
p->trapframe->epc = elf.entry;  // initial program counter = main
p->trapframe->sp = sp;          // user stack pointer
// return to user mode, and get the pc register poite to elf.entry, which is the entry point of the program "echo".



// Then it sets up registers and jumps to the trampoline page:
// set S Exception Program Counter to the saved user pc.
w_sepc(p->trapframe->epc);

// tell trampoline.S the user page table to switch to.
uint64 satp = MAKE_SATP(p->pagetable);




(gdb) p/x $satp
$37 = 0x8000000000087fff
(gdb) p/x $sepc
$38 = 0x0

(gdb) p/x fn
$39 = 0x3ffffff090

(gdb) b *fn

(gdb) b userret
// GDB will set the breakpoint based on symbol offsets, not just virtual address.
// But this still might not trigger depending on QEMU’s mapping visibility.
```

##### Issue: Not stop at breakpoint at userret at 0x3ffffff090?
but when continue in gdb, the gdb do not stop at breakpoint at userret at 0x3ffffff090?
Because the trampoline page (at TRAMPOLINE) is not actually mapped in the kernel’s address space when you set the breakpoint.
When xv6 runs normally:
- The kernel page table does not map that trampoline page.
- The user page table does map it at TRAMPOLINE = 0x3ffffff000.

So when GDB (attached to QEMU via target remote) sets a breakpoint at 0x3ffffff090,
the kernel cannot see or execute that address yet — because the current page table (kernel page table) does not include that mapping.

The CPU executes ((void(*)(...))fn)(TRAPFRAME, satp);
- That function call jumps to the physical address of the trampoline.
- xv6 temporarily maps it and switches to the new user page table inside the trampoline.
- GDB doesn’t see that transition because it doesn’t track virtual-to-physical mapping changes.


##### Debug  useret

That function call actually enters the trampoline, which:
- Switches the hardware page table register satp to the new user page table.
- Restores user registers from trapframe (including sepc, sp, etc.).
- Executes sret.

```bash
# restore user a0, and save TRAPFRAME in sscratch
csrrw a0, sscratch, a0

# return to user mode and user pc.
# usertrapret() set up sstatus and sepc.
sret

# After sret, the CPU will:
# switch to the user page table (satp = p->trapframe->kernel_satp)
# set pc = trapframe->epc (the user program’s next instruction)
# set sp = trapframe->sp (user stack pointer)
# and resume execution at the user program’s entry point — e.g. _echo’s main.

(gdb) where
#0  0x0000003ffffff10e in ?? ()



```

##### Debug after sret

sret makes the CPU switch:
- from supervisor mode → user mode
- pc = sepc = trapframe->epc (set by exec to the ELF entry)
- sp = user stack (set by exec)

At this point, we are executing user code again, but now in the echo program!

```bash
(gdb)si
# That’s the entry of user space — _echo’s first instruction.
0x0000000000000000 in ?? ()

# Now load the user symbols:
# Then, once it executes sret, switch to user symbols:
# This time, it will succeed because:
# You’re executing user code,
# GDB sees memory starting at 0x0 as valid,
# The mapping is active (since satp switched to user pagetable).
(gdb) add-symbol-file user/_echo 0x0
add symbol table from file "user/_echo" at
        .text_addr = 0x0
Reading symbols from user/_echo...
Error in re-setting breakpoint 4: No symbol "fn" in current context.
```

#### 4. Debug echo
Every xv6 user program starts from _start (in user/initcode.S or user/ulib.c):

```bash
.globl _start
_start:
  call main
  call exit

```

So after the sret:
- The CPU starts executing _start (the ELF entry point).
- _start calls main(argc, argv).
- You are now inside echo’s main() function!

```c

(gdb) b echo
```




### 4. Returning to shell

`exit()` → traps to kernel → cleans up the process → `wait()` in the shell’s parent returns.
Then the shell loop continues and prompts you again:


---


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

`walkaddr` (kernel/vm.c:104) checks that the user-supplied virtual address is part of the process’s user address space, so programs cannot trick the kernel into reading other memory. 
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


# 4.6 Page Fault Baics
- https://mit-public-courses-cn-translatio.gitbook.io/mit6-s081/lec08-page-faults-frans/8.1-page-fault-basics
  
## 4.6.1 Page Fault Baics

page fault可以让这里的地址映射关系变得动态起来。通过page fault，内核可以更新page table，这是一个非常强大的功能。因为现在可以动态的更新虚拟地址这一层抽象，结合page table和page fault，内核将会有巨大的灵活性。我们接下来会看到各种各样利用动态变更page table实现的有趣的功能。

从硬件和XV6的角度来说，当出现了page fault，现在有了3个对我们来说极其有价值的信息，分别是：
- STVAL: 引起page fault的内存地址
- SCAUSE: 引起page fault的原因类型
- SEPC: 引起page fault时的程序计数器值，这表明了page fault在用户空间发生的位置


但是在那之前，首先，我们需要思考的是，什么样的信息对于page fault是必须的。或者说，当发生page fault时，内核需要什么样的信息才能够响应page fault。

1. 我们需要出错的虚拟地址，或者是触发page fault的源。
   可以假设的是，你们在page table lab中已经看过一些相关的panic，所以你们可能已经知道，当出现page fault的时候，XV6内核会打印出错的虚拟地址，并且这个地址会被保存在STVAL寄存器中。所以，当一个用户应用程序触发了page fault，page fault会使用与Robert教授上节课介绍的相同的trap机制，将程序运行切换到内核，同时也会将出错的地址存放在STVAL寄存器中。这是我们需要知道的第一个信息。

2. 我们需要知道的第二个信息是出错的原因，我们或许想要对不同场景的page fault有不同的响应。
   不同的场景是指，比如因为load指令触发的page fault、因为store指令触发的page fault又或者是因为jump指令触发的page fault。所以实际上如果你查看RISC-V的文档，在 SCAUSE（注，Supervisor cause寄存器，保存了trap机制中进入到supervisor mode的原因）寄存器的介绍中，有多个与page fault相关的原因。比如，13表示是因为load引起的page fault；15表示是因为store引起的page fault；12表示是因为指令执行引起的page fault。所以第二个信息存在SCAUSE寄存器中，其中总共有3个类型的原因与page fault相关，分别是读、写和指令。ECALL进入到supervisor mode对应的是8，这是我们在上节课中应该看到的SCAUSE值。基本上来说，page fault和其他的异常使用与系统调用相同的trap机制（注，详见lec06）来从用户空间切换到内核空间。如果是因为page fault触发的trap机制并且进入到内核空间，STVAL寄存器和SCAUSE寄存器都会有相应的值。

3. 我们或许想要知道的第三个信息是触发page fault的指令的地址。
   从上节课可以知道，作为trap处理代码的一部分，这个地址存放在 SEPC（Supervisor Exception Program Counter）寄存器中，并同时会保存在trapframe->epc（注，详见lec06）中。


## 4.6.2 Lazy page allocation

在XV6中，sbrk的实现默认是eager allocation。这表示了，一旦调用了sbrk，内核会立即分配应用程序所需要的物理内存。但是实际上，对于应用程序来说很难预测自己需要多少内存，所以通常来说，应用程序倾向于申请多于自己所需要的内存。这意味着，进程的内存消耗会增加许多，但是有部分内存永远也不会被应用程序所使用到。

原则上来说，这不是一个大问题。但是使用虚拟内存和page fault handler，我们完全可以用某种更聪明的方法来解决这里的问题，这里就是利用lazy allocation。核心思想非常简单，sbrk系统调基本上不做任何事情，唯一需要做的事情就是提升p->sz，将p->sz增加n，其中n是需要新分配的内存page数量。但是内核在这个时间点并不会分配任何物理内存。之后在某个时间点，应用程序使用到了新申请的那部分内存，这时会触发page fault，因为我们还没有将新的内存映射到page table。所以，如果我们解析一个大于旧的p->sz，但是又小于新的p->sz（注，也就是旧的p->sz + n）的虚拟地址，我们希望内核能够分配一个内存page，并且重新执行指令。

所以，当我们看到了一个page fault，相应的虚拟地址小于当前p->sz，同时大于stack，那么我们就知道这是一个来自于heap的地址，但是内核还没有分配任何物理内存。所以对于这个page fault的响应也理所当然的直接明了：在page fault handler中，通过kalloc函数分配一个内存page；初始化这个page内容为0；将这个内存page映射到user page table中；最后重新执行指令。比方说，如果是load指令，或者store指令要访问属于当前进程但是还未被分配的内存，在我们映射完新申请的物理内存page之后，重新执行指令应该就能通过了。

我们首先要修改的是sys_sbrk函数，sys_sbrk会完成实际增加应用程序的地址空间，分配内存等等一系列相关的操作。

```c
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  // ============ demo code: page faults for lazy allocation ============ 
  myproc()->sz = myproc()->sz + n;
  // if(growproc(n) < 0)
  //   return -1;
  return addr;
}

```


```bash
$ echo hi
usertrap(): unexpected scause 0x000000000000000f pid=3
            sepc=0x0000000000001af8 stval=0x0000000000004008
```

我们可以查看Shell的汇编代码，这是由Makefile创建的。我们搜索SEPC对应的地址，可以看到这的确是一个store指令。这看起来就是我们出现page fault的位置。

```asm
# in use/sh.asm
hp->s.size = nu;
  1af0:       fe043783                ld      a5,-32(s0)
  1af4:       fdc42703                lw      a4,-36(s0)
  1af8:       c798                    sw      a4,8(a5)
            
```

首先查看trap.c中的usertrap函数，usertrap在lec06中有介绍。在usertrap中根据不同的SCAUSE完成不同的操作。


现在我们需要增加一个检查，判断SCAUSE == 15，如果符合条件，我们需要一些定制化的处理

```c
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
   } 
  // ============ demo code: page faults for lazy allocation ============ 
  // the page fault handler
  // when a page fault occurs, we need to allocate a page and map it to the faulting address.
    else if(r_scause()==15){
    uint64 va = r_stval();
    printf("page fault %p\n", va);
    uint64 ka = (uint64) kalloc();
    if(ka==0){
      p->killed = 1;
    }else{
      memset((void *) ka, 0 , PGSIZE);
      va = PGROUNDDOWN(va);
      if(mappages(p->pagetable, va, PGSIZE,ka,PTE_W|PTE_U|PTE_R)!=0){
        kfree((void *)ka);
        p->killed = 1;
      }
    }
  }
  else { 
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

```


接下来运行一些这部分代码。先重新编译XV6，再执行“echo hi”，我们或许可以乐观的认为现在可以正常工作了。
```bash
$ echo hi
page fault 0x0000000000004008
page fault 0x0000000000013f48
panic: uvmunmap: not mapped
```

但是实际上并没有正常工作。我们这里有两个page fault，第一个对应的虚拟内存地址是0x4008，但是很明显在处理这个page fault时，我们又有了另一个page fault 0x13f48。现在唯一的问题是，uvmunmap在报错，一些它尝试unmap的page并不存在。

这里unmap的内存是什么？ 之前lazy allocation但是又没有实际分配的内存。

为什么第二个的panic会存在？对于未修改的XV6，永远也不会出现用户内存未map的情况，所以一旦出现这种情况需要panic。但是现在我们更改了XV6，所以我们需要去掉这里的panic，因为之前的不可能变成了可能。

第二个的panic表明，我们尝试在释放一个并没有map的page。怎么会发生这种情况呢？唯一的原因是sbrk增加了p->sz，但是应用程序还没有使用那部分内存。因为对应的物理内存还没有分配，所以这部分新增加的内存的确没有映射关系。我们现在是lazy allocation，我们只会为需要的内存分配物理内存page。如果我们不需要这部分内存，那么就不会存在map关系，这非常的合理。相应的，我们对于这部分内存也不能释放，因为没有实际的物理内存可以释放，所以这里最好的处理方式就是continue，跳过并处理下一个page。






```c
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
      // ============ demo code: page faults for lazy allocation ============ 
      // when using lazy allocation, the page fault handler will allocate the page and update the PTE 
      //  continue;
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

```

## 4.6.3 Zero Fill On Demand

当你查看一个用户程序的地址空间时，存在text区域，data区域，同时还有一个BSS区域（注，BSS区域包含了未被初始化或者初始化为0的全局或者静态变量）。当编译器在生成二进制文件时，编译器会填入这三个区域。text区域是程序的指令，data区域存放的是初始化了的全局变量，BSS包含了未被初始化或者初始化为0的全局变量。

之所以这些变量要单独列出来，是因为例如你在C语言中定义了一个大的矩阵作为全局变量，它的元素初始值都是0，为什么要为这个矩阵分配内存呢？其实只需要记住这个矩阵的内容是0就行。

在一个正常的操作系统中，如果执行exec，exec会申请地址空间，里面会存放text和data。因为BSS里面保存了未被初始化的全局变量，这里或许有许多许多个page，但是所有的page内容都为0。

通常可以调优的地方是，我有如此多的内容全是0的page，在物理内存中，我只需要分配一个page，这个page的内容全是0。然后将所有虚拟地址空间的全0的page都map到这一个物理page上。这样至少在程序启动的时候能节省大量的物理内存分配

当然这里的mapping需要非常的小心，我们不能允许对于这个page执行写操作，因为所有的虚拟地址空间page都期望page的内容是全0，所以这里的PTE都是只读的。之后在某个时间点，应用程序尝试写BSS中的一个page时，比如说需要更改一两个变量的值，我们会得到page fault。那么，对于这个特定场景中的page fault我们该做什么呢？

应该创建一个新的page，将其内容设置为0，并重新执行指令。


## 4.6.4 Copy On Write Fork

## 4.6.5 Demand Paging
我们回到exec，在未修改的XV6中，操作系统会加载程序内存的text，data区域，并且以eager的方式将这些区域加载进page table。

但是根据我们在lazy allocation和zero-filled on demand的经验，为什么我们要以eager的方式将程序加载到内存中？为什么不再等等，直到应用程序实际需要这些指令的时候再加载内存？程序的二进制文件可能非常的巨大，将它全部从磁盘加载到内存中将会是一个代价很高的操作。又或者data区域的大小远大于常见的场景所需要的大小，我们并不一定需要将整个二进制都加载到内存中。

所以对于exec，在虚拟地址空间中，我们为text和data分配好地址段，但是相应的PTE并不对应任何物理内存page。对于这些PTE，我们只需要将valid bit位设置为0即可。

##  4.6.6 Memory Mapped Files