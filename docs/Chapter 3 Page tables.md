# Preparation: 

Read Chapter 3 and kernel/memlayout.h, kernel/vm.c, kernel/kalloc.c, kernel/riscv.h, and kernel/exec.c

# Introduction


`Page tables` are the most popular mechanism through which the operating system provides each process with its own `private address space and memory`. 
Page tables determine what memory addresses mean, and what parts of physical memory can be accessed. 
They allow xv6 to isolate different process’s address spaces and to multiplex them onto a single physical memory. 
Page tables are a popular design because they provide a level of indirection that allow operating systems to perform many tricks. xv6 performs a few tricks: mapping the same memory (a trampoline page) in several address spaces, and guarding kernel and user stacks with an unmapped page. 


The rest of this chapter explains the page tables that the RISC-V hardware provides and how xv6 uses them.

page tables provide `a level of indirection` to map virtual addresses to physical addresses.
this mapping is compelted under the control of the operating system.

## Terms

### Physical memory
- Physical memory refers to storage cells in DRAM. 
- A byte of physical memory has an address, called a `physical address`. 
- Instructions use only `virtual addresses`, which the paging hardware translates to physical addresses, and then sends to the DRAM hardware to read or write storage. 
- Unlike physical memory and virtual addresses, virtual memory isn’t a physical object, but refers to the collection of abstractions and mechanisms the kernel provides to manage physical memory and virtual addresses.


### Virtual address
Virtual address = [Virtual Page Number | Page Offset]

- `Virtual Page Number` (high bits):  Only the Virtual page number is looked up in the page table.
- `Offset` (low bits,12 bits → 0–4095): The offset is copied unchanged → this makes translations efficient

### Page table
- A page table is a data structure in memory used by the OS + hardware Memory Management Unit(MMU).
- It defines, for each `virtual page number (VPN)`, which `physical page number (PPN)` it maps to.
- Virtual Address (VA)   -->   Page Table   -->   Physical Address (PA)

Q: Does each process have a different page table?
- Each process has its own page table root (`satp register` in RISC-V points to it) → isolation.
- That page table describes the virtual→physical mapping for that process only.
- When xv6 switches from process A to process B, it switches the satp to point to B’s page table root.
- Same virtual address in different processes usually → different physical addresses.

Q: When can two processes map to the same physical page?
sometimes intentional sharing happens:
- fork(): child inherits parent’s page table. Pages are shared until one writes (Copy-on-Write).
- Shared memory (mmap/IPC): OS maps the same PA into multiple processes’ VAs.
- OS can explicitly arrange sharing (e.g., fork, shared memory).

### Page
- A page is a fixed-size block of virtual memory. On most systems, the default `page size = 4K= 4096 bytes = $2^12$ bytes`
- Page offset = the bottom `12 bits` of the virtual address，This means all addresses in a page differ only in these 12 bits.
- one page = 4096 contiguous virtual addresses, aligned on a 4KB boundary.
- “Granularity” means that the operating system can only manage memory mappings in these fixed chunks, not at an arbitrary byte level.



# 3.1 Paging hardware
The RISC-V page table hardware connects these two kinds of addresses, by mapping each virtual address to a physical address.
- `The Memory Management Unit (MMU)` translates every memory access.
- MMUs use page granularity:


## the Single-level MMU design
![image](../images/Figure%203.1-RISC-V%20virtual%20and%20physical%20addresses,%20with%20a%20simplified%20logical%20page%20table.png)

xv6 runs on Sv39 RISC-V, which means that 
- the top `25 bits` are not used.
- only the bottom `39 bits` of a 64-bit virtual address are used; 
  39 bits= 27 bits for index + 12 bits for offset
 
- In this Sv39 configuration, a RISC-V page table is logically an array of `2^27` (134,217,728) `page table entries (PTEs)`.
- Each PTE contains a `44-bit physical page number (PPN)` and some `flags`.
- The paging hardware translates a virtual address by using the top 27 bits of the 39 bits to index into the page table to find a PTE
- making a `56-bit physical address` whose top `44 bits` come from the `PPN` in the PTE and whose bottom `12 bits` are copied from the original virtual address.
- A page table gives the operating system control over virtual-to-physical address translations at the granularity of `aligned chunks of 4096 (2^12) bytes`. Such a chunk is called a `page`.


### Cons:
the page table size = 2^27 is too large，it is one giant flat page table
Each entry = 8 bytes → needs 1 GB of memory for the page table for every process, even if the process only uses a few pages. Huge waste.

## The Three-level MMU design

![image](../images/Figure%203.2-RISC-V%20address%20translation%20details.png)

As Figure 3.2 shows, a RISC-V CPU translates a virtual address into a physical in three steps.

[ VPN2 (9 bits) | VPN1 (9 bits) | VPN0 (9 bits) | Page offset (12 bits) ]

VA[38:30]  → top-level index into top-level page table.  (512 entries)
VA[29:21]  → mid-level index into middle-level page table (512 entries)
VA[20:12]  → leaf-level index into the lowest-level page table. (512 entries)
VA[11:0]   → page offset

Each page table page holds 512 PTEs (8 bytes each), so fits perfectly in one 4KB page.

A page table is stored in physical memory as `a three-level tree`. 
The root of the tree is a `4096-byte(4KB)` page-table page that contains `512 PTEs`, which contain the physical addresses for page-table pages in the next level of the tree. 
Each of those pages contains `512 PTEs` for the final level in the tree.


- The paging hardware uses the `top 9 bits` of the 27 bits to select a PTE in the root page-table page,
- the `middle 9 bits` to select a PTE in a page-table page in the next level of the tree, 
- the `bottom 9 bits` to select the final PTE. 

(In Sv48 RISC-V a page table has four levels, and bits 39 through 47 of a virtual address index into the top-level.)

If any of the three PTEs required to translate an address is not present, the paging hardware raises a` page-fault exception`, leaving it up to the kernel to handle the exception (see Chapter 4).

### The key idea: 
the first PPN in the top-level page diretory contains the physical page number (PPN) of the next-level down,
the final entry in the `final-level page directory` contains the actual physical page number (PPN) of the page that we acturally tring to translat to

`An invalid entry at level N `means you don’t need to allocate the next-level page table for that entire region of the address space.

1. What does a top-level entry represent?

In Sv39, the top-level page table (root) has 512 entries.
Each entry can either:
- Be valid, meaning it points to a second-level page table (another 4KB page that contains 512 PTEs).
- Be invalid, meaning this entire range of virtual addresses has no mapping.


2. What address range does one top-level entry cover?
               level  2     1     0    offset
   address num =     512 * 512 * 512 * 4096  = 512 *1024*1024*1024 = 512G

Each level indexes 9 bits = 512 possibilities.
- VPN[2] selects one of 512 PTEs in the root page table.
  Each entry covers a 512 * 512 * 4096 = 1 GB region (2³⁰).
- VPN[1] selects one of 512 entries in the next page table.
  Each entry covers a 512 * 4096= 2 MB region (2²¹).
- VPN[0] selects one of 512 entries in the final page table.
  Each entry covers a 4096 = 4 KB page (2¹²).
- Combine that with the page offset (12 bits), to get the actual physical address.


### Pros
The three-level structure of Figure 3.2 allows a `memory-efficient` way of recording PTEs, compared
to the single-level design of Figure 3.1.

In the common case in which large ranges of virtual addresses have no mappings, the three-level structure can omit entire page directories. 


For example, if an application uses only a few pages starting at address zero `VA=0,` 
1. Top-level page directory(root) has 512 entries.
  - Entry 0 points to the next-level table.
  - Entries 1–511 are invalid → kernel does not allocate those 511 second-level page tables.
  - Saved 511 × 1 page = 511 pages.
  
2. Second-level page directory (entry 0 only) also has 512 entries.
- Maybe the process uses just a handful of leaf pages at the very start.
- Only the first few entries are valid.
- For entries 1–511, the kernel does not allocate those third-level page tables.
- Each of those missing 511 entries would have required a bottom-level page table of 512 PTEs.
- Saved 511 × 512 = 261,632 pages.

So altogether:
Saved 511 mid-level tables
Saved 511 × 512 leaf-level tables

Savings = `511×4KB+(511×512)×4KB ~ 1 GB` saved in this example — the same size as the flat single-level table would’ve required!

and the kernel doesn’t have to allocate pages those for 511 intermediate page directories.
Furthermore, the kernel also doesn’t have to allocate pages for the bottom-level page directories for those 511 intermediate page directories. 
So, in this example, the three-level design saves 511 pages for intermediate page directories and 511 * 512 pages for bottom-level page directories.

### Flage in each PTE
Each PTE contains flag bits that tell the paging hardware how the associated virtual address
is allowed to be used.

- `PTE_V` indicates whether the PTE is present: if it is not set, a reference to the page causes an exception (i.e. is not allowed). 
- `PTE_R` controls whether instructions are allowed to read to the page. 
- `PTE_W` controls whether instructions are allowed to write to the page. 
- `PTE_X` controls whether the CPU may interpret the content of the page as instructions and execute them.
- `PTE_U` controls whether instructions in user mode are allowed to access the page; if PTE_U is not set, the PTE can be used only in supervisor mode.
Figure 3.2 shows how it all works. The flags and all other page hardware-related structures are defined in (kernel/riscv.h)


#### Flags in  non-leaf PTE vs  leaf PTE

1. A non-leaf PTE
A non-leaf PTE (also called a pointer PTE) doesn’t map directly. Instead, it points to another page table (the next lower level).

These entries must have PTE_V=1 but R/W/X = 0.

2. What is a “leaf PTE”?
A leaf PTE is one that directly maps a virtual page to a physical page.
In RISC-V, a leaf PTE must have at least one of the R/W/X permission bits set (PTE_R, PTE_W, PTE_X).
That means: “this PTE is a valid mapping to memory.”

Q: Why (pte & (PTE_R | PTE_W | PTE_X)) == 0 means not a leaf
Suppose you see a valid entry (pte & PTE_V).
If none of the R/W/X bits are set, the PTE is being used only as a pointer to another page table.
Therefore it’s not a leaf, so you must recurse into that next-level table.

This matches the RISC-V spec:
“If R=W=X=0 and V=1, then the PTE is a pointer to the next level of page table.”

If instead pte & (PTE_R|PTE_W|PTE_X) != 0, That means it’s a leaf entry.
You don’t recurse further, because this PTE maps directly to a physical page.

Example
```bash
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
# R/W/X bits are zero.
# So this is not a leaf → instead it points to another page table, which you then recurse into.

.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
# pte has the low bits ...1f → binary ends with 11111.
# That means V=1, R=1, W=1, X=1, U=1 → a leaf mapping.
# So the recursion stops here.

```

###  satp Register
To tell the hardware to use a page table, the kernel must write the `physical address` of `the root(top-level) page-table page` into the `satp` register. 
- Each CPU has its own satp(exactly one satp register). A CPU will translate all addresses generated by subsequent instructions using the page table pointed to by its own satp.
- Each CPU has its own satp so that different CPUs can run different processes, each with a private address space described by its own page table.
- The satp register tells the MMU: “here is the root of the page table I should use to translate virtual addresses.”
- So at any given instant, a CPU can only use one page table → meaning it can only run one process’s address space at a time.
- every time we swithc processes, we need to update/switch the satp register to point to the new process’s page table.


### Translation Look-aside Buffer (TLB)
To avoid the cost of loading PTEs from physical memory, a RISC-V CPU caches page table entries
in a `Translation Look-aside Buffer (TLB)`.

when the switch process, the kernel must flush the TLB.


### How different processes share the same CPU (context switching)
Different processes on the same CPU have different page tables because the kernel rewrites satp on `every context switch`.

When xv6 switches the CPU from running process A to process B:
1. Scheduler chooses process B.
2. Kernel saves A’s registers (including program counter, stack pointer, etc.) into A’s struct proc.
3. Kernel loads B’s registers.
4. Kernel writes B’s page table root physical address into satp.

This is the key step:
```c
// in xv6: proc.c, function `switchuvm(p)` (RISC-V)
w_satp(MAKE_SATP(p->pagetable));  // load satp with new root
sfence_vma();                     // flush TLB
```
5. CPU now uses B’s page table for address translation.

So different processes on the same CPU get different page tables because the OS updates the satp register during every context switch.





3. Why "invalid entry" means "no need to allocate"?
Suppose a process only uses a few MB near address 0x0000.
Only entry 0 in the top-level table is relevant.
Entries 1–511 cover virtual addresses far beyond what the process ever touches.
If the kernel marked them all invalid (valid=0 in PTE), the hardware will stop translation immediately when it sees an invalid entry.

That means the kernel does not need to allocate a second-level page table for those entries, because the hardware won’t ever need to look there.


Q: why are the physical page numbers in the PTEs stored in these `page directories`, rather than virtual addresses?
A:

Q: what does the satp register store? Does it store a pyhysical address or a virtual address?
A: The satp register stores a physical address.










# 3.2 Kernel address space
![image](../images/Figure%203.3-%20xv6's%20kernel%20address%20space.png)

## The Right part: Physical memory in DRAM
- `VIRTIO disk`: 0x10001000,  Virtual I/O Device Interface
- `UART`:        0x10000000L, Universal Asynchronous Receiver/Transmitter
- `PLIC`:        0x0c000000L, Platform Level Interrupt Controller
- `CLINT`:       0x2000000L,  Core Level Interrupt Controller
- `0x1000`:                   the pyshical address of boot ROOM
  when you turn on the board, the first thing the board does is to jump to the boot ROM, run the code inthe boot ROM, and then jump to` 0x8000 0000` the kernel.
- `0x0000`:                     unused
  
## The Left part: Kernel address space

The kernel address space is the part of virtual memory that the kernel uses.

In xv6 (RISC-V Sv39):  `kernel’s address space` vs `user address space`
- The kernel has its own separate page table, built at boot.
- a single page table that describes the `kernel’s address space`. 
- The kernel configures the layout of its address space to give itself access to physical memory and various hardware resources at predictable virtual addresses.

- Xv6 maintains `one page table per process`, User processes have their own page table (private address space).
- describing each process’s `user address space`

The kernel’s address space maps:
- Physical memory (so the kernel can access RAM directly).
- Device registers (memory-mapped I/O).
- Special regions like the trampoline and per-process kernel stacks.


### QEMU: KERNBASE & PHYSTOP
- QEMU simulates a computer that includes RAM (physical memory) starting at physical address `0x80000000` and continuing through at least `0x86400000`, which xv6 calls `PHYSTOP`.
- The QEMU simulation also includes I/O devices such as a disk interface. 
- QEMU exposes the device interfaces to software as memory-mapped control registers that sit below `0x80000000` in the physical address space. 
- The kernel can interact with the devices by reading/writing these special physical addresses; such reads and writes communicate with the device hardware rather than with RAM. 

Chapter 4 explains how xv6 interacts with devices.

### Direct mapping

The kernel gets at RAM and memory-mapped device registers using `“direct mapping`” that is, mapping the resources at virtual addresses that are **equal** to the physical address. 

For example, the kernel itself is located at `KERNBASE=0x80000000` in both the virtual address space and in physical memory. 
Direct mapping simplifies kernel code that reads or writes physical memory. 

For example, when fork allocates user memory for the child process, the allocator returns the physical address of that memory; 
fork uses that address directly as a virtual address when it is copying the parent’s user memory to the child

### Not direct-mapped
There are a couple of kernel virtual addresses that aren’t direct-mapped:
#### `The trampoline page`

The `trampoline` is a tiny piece of assembly code mapped at the very top of the virtual address space (TRAMPOLINE = MAXVA - PGSIZE).
It is mapped at the top of the virtual address space; user page tables have this same mapping. 

Purpose: Handle` trap/interrupt/exception` entry and return.
When the CPU switches from `user mode` → `supervisor mode` (on a system call, interrupt, or page fault), it jumps to this `trampoline` code.

The `trampoline` then sets up the `kernel stack` and jumps into `the C trap handler`.  

Benefit: Since it’s always at the same VA, user processes can safely execute an ecall → CPU jumps to the same known place.


Chapter 4 discusses the role of the trampoline page, but we see here an interesting use case of page tables; 
a physical page (holding the trampoline code) is `mapped twice` in the virtual address space of the kernel: once at top of the virtual address space and once with a direct mapping.


#### `The kernel stack pages` 
Each process has its own `kernel stack`(one page in xv6) for running in kernel mode
This stack is used only when the process is running in the kernel:
- Handling a system call.
- Handling a page fault.
- Handling an interrupt while running on that process’s CPU.

Why separate kernel stack?
- The user stack cannot be trusted (user could mess with it).
- Kernel needs a clean, safe stack when executing privileged code.
- Isolation: Each process needs its own kernel stack to maintain separate kernel execution contexts
- System Calls: When a process makes a system call, it needs stack space to execute kernel code
- Interrupts: Hardware interrupts need stack space to save state and execute handlers
- Security: Separate stacks prevent one process from corrupting another's kernel execution


#### `Guard Page` 
xv6 allocates one page for the stack plus one unused “guard page” below it.
- The guard page’s PTE is marked invalid (i.e., PTE_V is not set) in the kernel page table.
so that if the kernel overflows a kernel stack, it will likely cause an exception and the kernel will panic. Without a guard page an overflowing stack would overwrite other kernel memory, resulting in incorrect operation. A panic crash is preferable.



## Kernel Stacks

Kernel stacks are placed high in the kernel virtual address space, not in user space.
Stacks grow downward (from high to low addresses)

Each stack takes 2 pages of virtual address, Each stack is PGSIZE (4096 bytes):
- 1 valid page (the actual stack)
- 1 invalid guard page 
  unmapped, so an overflow causes a fault instead of silently corrupting memory
  Guard pages prevent stack overflow from corrupting adjacent stacks


The user address space stops at MAXVA (2^38).


### Memory Layout of Kernel Stacks
The kernel stacks are arranged in virtual memory like this:
```c
TRAMPOLINE (highest kernel address)
... 
[guard page - unmapped]
[kernel stack for proc[1]]
[guard page - unmapped]  
[kernel stack for proc[1]]
...


```


### When Kernel Stack is Used

When a user process traps into the kernel (e.g., system call, interrupt), the CPU switches from using the user stack (p->trapframe->sp) to the kernel stack (p->kstack).


1. During Trap Entry (user → kernel) in System Calls/Interrupts:
```c
// In trampoline.S - simplified view
// 1. CPU switches to kernel page table
// 2. Switches to kernel stack
csrw sscratch, a0        // Save user a0
ld sp, 8(a0)            // Load kernel stack pointer
// Now using kernel stack for execution
```


2. During Context Switch:
```c
// context structure saved on kernel stack
struct context {
    uint64 ra;  // Return address
    uint64 sp;  // Stack pointer
    // ... other saved registers
};

// swtch saves current context on kernel stack
// and restores another process's context
```

### Key Usage Patterns
1. Stack Pointer Management
```c
// In allocproc():
p->context.sp = p->kstack + PGSIZE;  // Start at top of stack

// During execution, SP moves down as functions are called:
function_call() {
    // SP decreases to make room for local variables
    // SP increases when function returns
}
```

2. Trap Handling Flow:
```
User Process Executing
    ↓ (trap/syscall/interrupt)
Save user state in trapframe
    ↓
Switch to kernel stack (sp = p->kstack + offset)
    ↓
Execute kernel code on kernel stack
    ↓
Restore user state from trapframe
    ↓
Return to user mode
```


# 3.3 Code: creating an address space

Most of the xv6 code for manipulating address spaces and page tables resides in vm.c (kernel/vm.c:1). 

## Structures & Functions

### `pagetable_t`
The central data structure is pagetable_t, which is really **a pointer to a RISC-V root page-table page**; a pagetable_t may be either the kernel page table, or one of the perprocess page tables. 


```c
typedef uint64 *pagetable_t; // points to 512 PTEs (one page table)，uint64 * is a pointer to a 64-bit unsigned integer
```

### `walk`
The central functions are walk, which finds the PTE for a virtual address,
and `mappages`, which installs PTEs for new mappings. 



### kvm
Functions starting with kvm manipulate the `kernel page table`; 

### uvm
functions starting with uvm manipulate a `user page table`; 

other functions are used for both. 

### copyout and copyin
copyout and copyin copy data to and from user virtual addresses provided as system call arguments; 
they are in vm.c because they need to explicitly translate those addresses in order to find the corresponding physical memory.


## Creating an address space

### `kvminit`

Early in the boot sequence, `main` calls `kvminit` (kernel/vm.c:54) to create the kernel’s page table using `kvmmake` (kernel/vm.c:20). 
This call occurs before xv6 has enabled paging on the RISC-V, so addresses refer directly to **physical memory**. 

- `kvmmake` first allocates a page of physical memory to hold `the root page-table page`. 
- Then it calls `kvmmap` to install the translations that the kernel needs. The translations include 
  - the kernel’s instructions 
  - data
  - physical memory up to PHYSTOP
  - memory ranges which are actually devices. 
- `proc_mapstacks` (kernel/proc.c:33) allocates a kernel stack for each process. 
  It calls `kvmmap` to map each stack at the virtual address generated by `KSTACK`, which leaves room for the invalid stack-guard pages.

### kvmmap
`kvmmap` (kernel/vm.c:127) calls `mappages` (kernel/vm.c:138), which installs mappings into a page table for a range of virtual addresses to a corresponding range of physical addresses. 
It does this separately for each virtual address in the range, at page intervals. 

For each virtual address to be mapped, `mappages` calls `walk` to find the address of the PTE for that address. 
It then initializes the PTE to hold the relevant physical page number, the desired permissions (PTE_W, PTE_X, and/or PTE_R), and PTE_V to mark the PTE as valid (kernel/vm.c:153).

### walk

`walk` (kernel/vm.c:81) mimics the RISC-V paging hardware as it looks up the PTE for a virtual address (see Figure 3.2).
`walk` descends the 3-level page table 9 bits at the time. It uses each level’s 9 bits of virtual address to find the PTE of either the next-level page table or the final page (kernel/vm.c:87). 
- If the PTE isn’t valid, then the required page hasn’t yet been allocated; 
- if the alloc argument is set, walk allocates a new page-table page and puts its physical address in the PTE. 
- It returns the address of the PTE in the lowest layer in the tree (kernel/vm.c:97).

The above code depends on physical memory being `direct-mapped into the kernel`


## Debug kvminit()

```bash
# #1 terminal
$ make CPUS=1 qemu-gdb


# #2 terminal
$ gdb-multiarch kernel/kernel
(gdb) target remote :26000
(gdb) b main
(gdb) b kvminit
(gdb) c

# pagetable_t kpgtbl = (pagetable_t) kalloc();  // allocate a physical page│
(gdb) p kpgtbl
$1 = (pagetable_t) 0x87fff000

# // uart registers
# kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
(gdb) set $UART0=0x10000000L
(gdb) p/x $UART0
$9 = 0x10000000
(gdb) set $PGSIZE=4096
(gdb) p $PGSIZE
$5 = 4096
(gdb)  p/x $UART0+$PGSIZE
$8 = 0x10001000

# // map kernel text executable and read-only.
# kvmmap(kpgtbl,  KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
(gdb) set $KERNBASE=0x80000000L
(gdb) p/x $KERNBASE
$10 = 0x80000000

# get the level 2 index for va=KERNBASE
(gdb) p/d ($KERNBASE>>30) & 0x3ff
$17 = 2
# test if the PTE is valid
(gdb) p/x kpgtbl[2] & 0x1
$20 = 0x1
# test if the PTE is leaf or not: not a leaf
(gdb) p/x kpgtbl[2] &  ((1<<1)|(1<<2)|(1<<3))
$21 = 0x0

(gdb) p etext
$11 = 0x8000b000 "cons"
(gdb) p/x etext
$12 = 0x8000b000
(gdb) p/x (unsigned long long)etext
$13 = 0x8000b000


(gdb) p/x (unsigned long long)etext-$KERNBASE
$15 = 0xb000


```


# 3.4 Physical memory allocation

The kernel must allocate and free physical memory at **run-time** for page tables, user memory, kernel stacks, and pipe buffers.
xv6 uses the physical memory between the end of the kernel and PHYSTOP for run-time allocation.
- It allocates and frees whole `4096-bytes= 4KB = 512 * 8 Bytes` pages at a time. 
- It keeps track of which pages are free by threading `a linked list` through the pages themselves. 
- Allocation consists of removing a page from the linked list; 
- freeing consists of adding the freed page to the list.



# 3.5 Code: Physical memory allocator
## The allocator’s data structure

```C
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

```
The allocator resides in kalloc.c (kernel/kalloc.c:1). 
The allocator’s data structure is a free list of physical memory pages that are available for allocation. 
Each free page’s list element is a `struct run` (kernel/kalloc.c:17). 

Where does the allocator get the memory to hold that data structure?
It store each free page’s run structure in the free page itself, since there’s nothing else stored there. The free list is protected by `a spin lock` (kernel/kalloc.c:21-24). 
The list and the lock are wrapped in a struct to make clear that the lock protects the fields in the struct. 

For now, ignore the lock and the calls to acquire and release; Chapter 6 will examine locking in detail.

## kalloc
`kalloc` removes and returns the first element in the free list.


When a process asks xv6 for more user memory, xv6 first uses `kalloc` to allocate physical pages. 
It then adds PTEs to the process’s page table that point to the new physical pages. 
Xv6 sets the PTE_W, PTE_X, PTE_R, PTE_U, and PTE_V flags in these PTEs. 
Most processes do not use the entire user address space; 
xv6 leaves PTE_V clear in unused PTEs.

We see here a few nice examples of use of page tables. 
1. different processes’ page tables translate user addresses to different pages of physical memory, so that each process has `private user memory`. 
2. each process sees its memory as having `contiguous virtual addresses` starting at zero, while the process’s physical memory can be `non-contiguous`. 
3. the kernel maps a page with `trampoline` code at the top of the user address space, thus a single page of physical memory shows up in all address spaces.



### Debugging for data structure and kalloc
```c
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;   // grabs the current 1st free page of the linked list of all free physical memory pages.
  if(r)                // if there is a free page (r!=0)
    kmem.freelist = r->next; // it moves the head pointer forward , we "consume" that page and mark it as no longer free.
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

```bash
# Print the head of the freelist
# Shows the address of the first free page. If it’s 0x0, the list is empty.
(gdb) p kmem.freelist
$22 = (struct run *) 0x87f6e000

# Print the next pointer of the head
(gdb) p kmem.freelist->next
$23 = (struct run *) 0x87f6d000

# GDB can directly tell you the size of a struct with the ptype or print sizeof command.
# That means struct run is 8 bytes long (just one pointer).
(gdb) ptype struct run
type = struct run {
    struct run *next;
}

(gdb) whatis r
type = struct run *

(gdb) p sizeof(struct run)
$25 = 8

# You can cast any pointer to void * if you just care about the raw address.
# But void * doesn’t allow arithmetic, because it has no element size.
# Cast to void * to see raw address
# Sometimes GDB tries to pretty-print as struct run *. You can force an address:
(gdb) p (void *)kmem.freelist
$24 = (void *) 0x87f6e000


# Shows that r is a pointer to struct run located at address 0x87f6e000.
(gdb) p r
$12 = (struct run *) 0x87f6e000

# p/x r - Shows the same address in hexadecimal format (0x87f6e000).
(gdb) p/x r
$15 = 0x87f6e000

# p *r - Dereferences the pointer and shows the content of the struct:
# This means that this free page points to the next free page in the linked list, which is at address 0x87f6d000.
(gdb) p *r
$13 = {
  next = 0x87f6d000
}

(gdb) p (struct run*)r
$17 = (struct run *) 0x87f6e000

# p *(struct run*)r - This is equivalent to p *r, explicitly casting r to struct run*.
(gdb) p *(struct run*)r
$14 = {
  next = 0x87f6d000
}

(gdb) p r->next
$21 = (struct run *) 0x87f6d000

# p *r->next - Dereferences the next pointer and shows:
# This demonstrates that you can follow the linked list from one free page to the next.
(gdb) p *r->next
$16 = {
  next = 0x87f6c000
}


# x/x r - Examines memory at address r in hexadecimal format, showing 0x87f6d000, which is the value of r->next.
(gdb) x/x r
0x87f6e000:     0x87f6d000

# x/x r->next - Examines memory at address r->next (0x87f6d000), showing 0x87f6c000, which is the value of the next pointer in the chain.
# What r->next means
# - If you have a pointer r of type struct run *,
# - Then r->next is the pointer to the next free page in the free list.
# Since each struct run is placed at the start of a free page (4 KB), r->next will be an address (pointer) of the next free page.
(gdb) x/x r->next
0x87f6d000:     0x87f6c000


# In GDB, pointer subtraction is allowed: ptr1 - ptr2 = the difference in array elements (not raw bytes, but number of objects of that type between the two pointers).
# - r and r->next are both struct run *.
# - r - r->next = how many struct run objects apart they are in memory.
# - Since each struct run occupies 1 page (4096 bytes), the subtraction corresponds to (address(r) - address(r->next)) / sizeof(struct run).
# But here sizeof(struct run) is probably 8 bytes (just one pointer).
# So the value 512 means: 
# (address(r) - address(r->next)) = 512 * sizeof(struct run) = 512 * 8 = 4096
# So $20 = 512 is telling you that r and r->next are 4096 bytes apart — i.e., one page.
# This confirms the free list links pages in memory separated by 4 KB.
(gdb) p r-r->next
$20 = 512
```


## kinit

The function main calls `kinit` to initialize the allocator (kernel/kalloc.c:27). 
kinit initializes the free list to hold every page between `the end of the kernel` and `PHYSTOP`. 
xv6 ought to determine how much physical memory is available by parsing configuration information provided by the hardware. 

Instead xv6 assumes that the machine has `128 megabytes of RAM`. 

`kinit` calls `freerange` to add memory to the free list via per-page calls to `kfree`. 
A PTE can only refer to a physical address that is aligned on a 4096-byte boundary (is a multiple of 4096), so freerange uses `PGROUNDUP` to ensure that it frees only aligned physical addresses. 
The allocator starts with no memory; these calls to `kfree` give it some to manage. 
The allocator sometimes **treats addresses as integers** in order to perform arithmetic on them (e.g., traversing all pages in freerange), and sometimes **uses addresses as pointers** to read and write memory (e.g., manipulating the run structure stored in each page); this dual use of addresses is the main reason that the allocator code is full of C type casts. 

The other reason is that freeing and allocation inherently change the type of the memory.




## kfree
The function `kfree` (kernel/kalloc.c:47) begins by setting every byte in the memory being freed to the value 1. 
This will cause code that uses memory after freeing it (uses “dangling references”) to read garbage instead of the old valid contents; hopefully that will cause such code to break faster.
Then kfree **prepends** the page to the free list: it casts pa to a pointer to struct run, records the old start of the free list in r->next, and sets the free list equal to r. 


   
# 3.6 Process address space


Each process has a separate page table, and when xv6 switches between processes, it also changes page tables. 

![image](../images/Figure%202.3-Layout%20of%20a%20process’s%20virtual%20address%20space.png)

As Figure 2.3 shows, a process’s user memory starts at virtual address zero and can grow up to `MAXVA` (kernel/riscv.h:360), allowing a process to address in principle `256 Gigabytes of memory`.

```bash
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
1L<<38= 2^38= 2^8 * 2^30= 256GB
```
![images](../images/Figure%203.4-A%20process’s%20user%20address%20space,%20with%20its%20initial%20stack.png)

## Address Space Layout

Typical layout looks like this:

```
   high addresses
   ────────────────────────────── MAXVA
   trampoline   (shared page for traps/switching to kernel)
   ──────────────────────────────
   trapframe    (per-process kernel data page)
   ──────────────────────────────
   user stack   (one page + guard page below)
   ──────────────────────────────
   program heap (grows upward via sbrk)
   ──────────────────────────────
   program text, data, bss (loaded from ELF at low addresses)
   ────────────────────────────── 0x0
   low addresses
```

Figure 3.4 shows the layout of the user memory of an executing process in xv6 in more detail.

user stack:  one page + guard page below
  
The stack is `a single page`, and is shown with the initial contents as created by `exec`. 
Strings containing the command-line arguments, as well as an array of pointers to them, are at the very top of the stack. 
Just under that are values that allow a program to start at `main` as if the function main(argc, argv) had just been called.

To detect a user stack overflowing the allocated stack memory, xv6 places an inaccessible `guard page` right below the stack by clearing the `PTE_U` flag. 
If the user stack overflows and the process tries to use an address below the stack, the hardware will generate a page-fault exception because the guard page is inaccessible to a program running in user mode. 

A real-world operating system might instead automatically allocate more memory for the user stack when it overflows.

---




## How to building a process address space

In xv6, a **process address space** is built when:

- the first user process (`init`) starts
  or
- you run `exec()` to load a new program into an existing process.

## Debug procinit


```c
// initialize the proc table at boot time.
// In unmodified xv6, all the kernel stacks are set up in `procinit`, 
// here set up kernel stacks for all possible processes.
// All kernel stacks are mapped in the single global kernel page table
// Every process sees all other processes' kernel stacks (though they don't use them)


// In your lab3-part modification:
// Kernel stack allocation moves from procinit() to allocproc() (when process is created)
// Each process's kernel stack is mapped only in its own kernel page table
// This provides better isolation between processes
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  // proc is a statically allocated array of NPROC (typically 64) process structures
  for(p = proc; p < &proc[NPROC]; p++) {
      // Initializes each process's individual lock
      initlock(&p->lock, "proc");

      // Allocate a page for the process's kernel stack.
      // Map it high in memory, followed by an invalid
      // guard page.
      
      // Allocates a physical page (4096 bytes) for this process slot's kernel stack
      // kalloc() returns the physical address of the allocated page
      char *pa = kalloc(); 
      if(pa == 0)
        panic("kalloc");
      
      // Kernel stacks are placed high in the kernel virtual address space, not in user space.
      // Calculates the virtual address for this process's kernel stack    
      // (p - proc) gives the process index (0 to NPROC-1)
      // KSTACK() macro (defined in memlayout.h) computes the virtual address:
      // Each kernel stack is placed below TRAMPOLINE with a guard page between them

      uint64 va = KSTACK((int) (p - proc));

      //Maps the physical page (pa) to virtual address (va) in the global kernel page table
      // PTE_R | PTE_W flags make it readable and writable (not executable)
      // This creates the actual page table entry
      kvmmap(kernel_pagetable, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
      
      // Stores the kernel stack's virtual address in the process structure
      // so the kernel knows where this stack lives.
      // This will be used later when context switching to this process
      p->kstack = va;
  }
  kvminithart();
}

// The first process structure (proc[0]) is located in memory at address 0x80014d58
// This is a physical memory address in the kernel's address space (since xv6 is a unikernel operating system)
// The address 0x80014d58 falls within the kernel's memory space.
(gdb) p &proc[0]
$50 = (struct proc *) 0x80014d58 <proc>
(gdb) p &proc[1]
$48 = (struct proc *) 0x80014ed0 <proc+376>
(gdb) p &proc[2]
$49 = (struct proc *) 0x80015048 <proc+752>

(gdb) p/x &proc
$55 = 0x80014d58

(gdb) p/x &proc[0]
$57 = 0x80014d58

(gdb) p p
$26 = (struct proc *) 0x8000d000 <stack0+4048>

// char *pa = kalloc();
(gdb) p pa
// $27 = 0x87fb6000 '\005' <repeats 200 times>...

// uint64 va = KSTACK((int) (p - proc));
// #define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

(gdb) p/x p
$63 = 0x80014ed0
(gdb) p/x &proc
$62 = 0x80014d58

(gdb) p/x (int)(p-proc)
$64 = 0x1


(gdb) set $MAXVA=1L<<38
(gdb) p/x $MAXVA

(gdb) set $TRAMPOLINE= $MAXVA-$PGSIZE
(gdb) p/x $TRAMPOLINE
$41 = 0x3ffffff000
(gdb) p/x $TRAMPOLINE - 2*$PGSIZE
$42 = 0x3fffffd000

(gdb) p/x va
$43 = 0x3fffffd000

// p->kstack = va;
(gdb) p/x p->kstack
$46 = 0x3fffffd000
```





### Debug kvminithart
```c
// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}
```




# 3.7 Code: sbrk

# First user process: userinit()

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



## The `init` program's behavior

Once `init` starts running, it typically:
- Opens file descriptors 0, 1, and 2 (stdin, stdout, stderr) connected to the console
- Forks child processes to run shell instances
- Waits for child processes and restarts them if they exit
- Essentially acts as the parent of all user processes

The transition from kernel mode to user mode for the first time is a critical moment in the boot process - it marks the point where the system moves from pure kernel execution to having actual user programs running.

This design follows the traditional Unix model where `init` (process ID 1) is the root of the entire process tree in user space.



## Debug First user process
### Debug main
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

### Debug userinit

```c
// a user program that calls exec("/init")
// od -t xC initcode
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

### Debug allocproc
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


### Debug scheduler

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

// ra (Return Address) = 0x800035b2 <forkret>
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

### Debug forkret

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

### Debug usertrapret in trap.c

```c
/
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
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
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
```

### Debug uservec in trampoline.S (see chapter4 )

```c
// In trampoline.S - simplified view
// 1. CPU switches to kernel page table
// 2. Switches to kernel stack
csrw sscratch, a0        // Save user a0
ld sp, 8(a0)            // Load kernel stack pointer
// Now using kernel stack for execution
```


### Debug usertrap in trap.c

```c
//
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

```

### Debug syscall->exec -> usertrapret

#### syscall (see chapter 2)

#### exec (see 3.8 Code:exec)

#### usertrapret（see above）




```c
// in exec()
(gdb) p/s path
$9 = 0x3fffffdf08 "/init"

```


# 3.8 Code: exec

`Exec` is the system call that creates the user part of an address space. 

## Big Picture
Goal: Tear down the old process’s memory image and replace it with a new one loaded from an ELF file on disk.


## 1. Look up ELF(executable file)
```C
ip = namei(path);
ilock(ip);
readi(ip, 0, (uint64)&elf, 0, sizeof(elf));
```

It initializes the user part of an address space from a file stored in the file system. 
- Exec (kernel/exec.c:13) opens the named binary path using `namei` (kernel/exec.c:26), which is explained in Chapter 8. 
- Then, it reads the `ELF header`(first few bytes of the file) 
  Xv6 applications are described in the widely-used ELF format, defined in (kernel/elf.h).  
  An ELF binary consists of 
  - a. an ELF header `struct elfhdr` (kernel/elf.h:6)
  - b. followed by a sequence of program section headers `struct proghdr` (kernel/elf.h:25). 
    Each proghdr describes a section of the application that must be loaded into memory; 
    xv6 programs have only one program section header, but other systems might have separate sections for instructions and data.
  
- Verifies it is a valid ELF (elf.magic == ELF_MAGIC).
  
The first step is a quick check that the file probably contains an ELF binary. 
An ELF binary starts with the four-byte “magic number” 0x7F, ‘E’, ‘L’, ‘F’, or ELF_MAGIC (kernel/elf.h:3). 
If the ELF header has the right magic number, exec assumes that the binary is well-formed.


## 2. Build a fresh page table and allocate trampoline and trapframe pages

Create an empty page table: 
- Exec allocates a new page table with no user mappings with `proc_pagetable` (kernel/exec.c:38)
- `uvmcreate()` → allocates a top-level (level-2) page table.   This gives us a clean slate for user virtual memory.

```c
pagetable = proc_pagetable(p);

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.  allocates a top-level (level-2) page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // At the very top of user address space
  // - Trampoline page: contains code to enter/exit the kernel on ecall
  // - Trapframe page: per-process data (registers saved during a trap)
  // These are special mappings so the user process can enter the kernel safely.

  // map the trampoline code (for system call return) at the highest user virtual address.
  // only the supervisor uses it, on the way to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}
(gdb) b exec
(gdb) next
....

(gdb) p/s path
$3 = 0x3fffffdf08 "/init"
(gdb) p/s argv[0]
$4 = 0x87f6f000 "/init"
(gdb) p/s argv[1]
$5 = 0x0


// An empty page table.  allocates a top-level (level-2) page table.
// pagetable = uvmcreate();
(gdb) p pagetable
$32 = (pagetable_t) 0x87f6e000
(gdb) ptype pagetable
type = unsigned long *

// #define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
// user space: 0 ~ MAXVA:  1L<<38= 2^38= 2^8 * 2^30= 256GB
(gdb) p/x 1L << (9 + 9 + 9 + 12 - 1)
$9 = 0x4000000000
(gdb) p/x 4096
$18 = 0x1000

// #define TRAMPOLINE (MAXVA - PGSIZE)
// TRAMPOLINE is a constant virtual address, defined in memlayout.h as the very top of user space.
// TRAMPOLINE: top address of process virtual space
(gdb)  p/x (1L<<38)-4096
$11 = 0x3ffffff000

// get index of the level-2 page table entry for TRAMPOLINE
// TRAMPOLINE = (1<<38)-0x1000 lies in the last valid user page,
// There are only 256 valid top-level slots for user space (0–255). The other 256 entries (256–511) correspond to kernel or invalid addresses due to sign-extension.
(gdb) p/d ((1L<<38)-4096)>>30
$5 = 255

// trampoline is a label in trampoline.S, i.e., the start of the assembly code that handles traps (uservec, userret, etc.).
// (uint64)trampoline is the physical address of that code in kernel memory.
(gdb) p/x trampoline
$4 = 0x8000a000 <uservec>

// gives you the physical (kernel) address.
(gdb) info address trampoline
Symbol "trampoline" is static storage at address 0x8000a000.

// Disassemble the trampoline code
(gdb) disas trampoline
Dump of assembler code for function uservec:
   0x000000008000a000 <+0>:     csrrw   a0,sscratch,a0
   0x000000008000a004 <+4>:     sd      ra,40(a0)
   0x000000008000a008 <+8>:     sd      sp,48(a0)
   0x000000008000a00c <+12>:    sd      gp,56(a0)
   0x000000008000a010 <+16>:    sd      tp,64(a0)
   0x000000008000a014 <+20>:    sd      t0,72(a0)
   0x000000008000a018 <+24>:    sd      t1,80(a0)
   0x000000008000a01c <+28>:    sd      t2,88(a0)
   0x000000008000a020 <+32>:    sd      s0,96(a0)
   0x000000008000a022 <+34>:    sd      s1,104(a0)

(gdb) x/10i trampoline
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
  
  
// mappages creating a mapping:
// Virtual address (TRAMPOLINE) → Physical address of trampoline code.
// This way, the trampoline code is mapped inside every user page table at the same virtual address, but all point to the same single physical page in the kernel.
```
### Debug for uvmcreate
```c
// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}
```

### Debug for mappages

```c
// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;
  //PGROUNDDOWN removes the page offset, so a is always aligned.
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    //This stores only the physical page number (pa >> 12)
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

(gdb) p/x va
$45 = 0x3ffffff000 // TRAMPOLINE: top virtual address of user process space
(gdb) p/x size
$46 = 0x1000

(gdb) p/x pa
$47 = 0x8000a000   // physical address of trampoline code

(gdb) x/10i pa
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

(gdb) p/t pa
$55 = 10000000000000001010000000000000
(gdb) p/t pa>>12
$56 = 10000000000000001010
(gdb) p/t (pa>>12)<<10
$57 = 100000000000000010100000000000

// va-> a: align vitual address
(gdb) p/x a
$44 = 0x3ffffff000
(gdb) p/x last
$41 = 0x3ffffff000




////////////////////////////////////////////////////////////
// get the PTE from walk(): the pte after walk is a pointer
// if((pte = walk(pagetable, a, 1)) == 0) 
(gdb) p pte
$78 = (pte_t *) 0x87f6cff8
// pte is a pointer to None
(gdb) p *pte
$79 = 0 //not mapping yet

// pte to point to a PTE:  *pte = PA2PTE(pa) | perm | PTE_V;
// assign the final pte point to the acutual phyical address num (PPN):
// shift a physical address to the right place + flags
// #define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
(gdb) p/t pa
$87 = 10000000000000001010000000000000
(gdb) p/t pa>>12
$88 = 10000000000000001010
(gdb) p/t (pa>>12)<<10
$89 = 100000000000000010100000000000
(gdb) p/t perm
$90 = 1010
(gdb) p/x perm
$92 = 0xa
(gdb) p/t perm | 0x1
$99 = 0xb
(gdb) p/t perm | 0x1
$100 = 1011
(gdb) p/t ((pa>>12)<<10) | 0xb
$93 = 10000000000000001010000000101
(gdb) p/x ((pa>>12)<<10) | 0xb
$102 = 0x2000280b

// pte is a pointer to the PTE
// its address: from the 3 level page table by virtural address
// it points to: the PTE(shifted physical address + flags)
(gdb) p pte
$104 = (pte_t *) 0x87f6cff8

(gdb) p/x *pte
$98 = 0x2000280b

// by this PTE, we can get the physical address num
// when PPN + the offset bit, we can get the physical address
(gdb) p/x (0x2000280b>>10)<<12
$8 = 0x8000a000
```

### Debug for walk
```c
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    // PX(level, va):  get the index ofthat level page table by extracting the right 9 bits 
    // pagetable[PX(level, va)]: get the entry of that level page table at index
    // &pagetable[PX(level, va)]: get the addrees of that enry and store it in pte,so pte points to the entry in the current page table
    pte_t *pte = &pagetable[PX(level, va)]; 
    if(*pte & PTE_V) {
      //if pte value is valid: follow the pointer to next-level page table
      //PTE2PA(*pte):   extracts the physical address stored in the PTE.
      //(pagetable_t)PTE2PA(*pte): Cast it to pagetable_t, so we can treat that physical page as another page table.
      pagetable = (pagetable_t)PTE2PA(*pte); 
    } else {
      //if pte value is not valid: allocate a new page-table page
      // If alloc == 0, we can’t proceed → return 0 (failure).
      // Otherwise, allocate a new empty page table page with kalloc().
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      // only storing the PPN (page frame number) from phsical address and permission bits to the current PTE
      *pte = PA2PTE(pagetable) | PTE_V; 
    }
  }
  // After the loop, we’re at level 0 (leaf).
  // Return the pointer to the leaf PTE that corresponds to va.
  return &pagetable[PX(0, va)];
}


// pagetable is the physical address of a page table page (like 0x87f6e000). 
(gdb) p pagetable
$82 = (pagetable_t) 0x87f6e000

// the trampoline virtual address
(gdb) p/x va
$61 = 0x3 ff ff ff 000
(gdb) p/t va
$89 = 11 11111111 11111111 11111111 0000 0000 0000

// PX(level, va):  extracts the right 9 bits for that level  ->  index in that level page table 
// PGSHIFT = 12 → (because page size = 2^12 = 4096).
// At level=2 → PXSHIFT(2) = 12 + 18 = 30.
// At level=1 → PXSHIFT(1) = 21.
// At level=0 → PXSHIFT(0) = 12. 
// So:
// PX(2, va) extracts VPN2 (bits 38–30): 255
// PX(1, va) extracts VPN1 (bits 29–21): 511
// PX(0, va) extracts VPN0 (bits 20–12): 511


// for level 2 page table 
// get the level 2 page table index
(gdb) p/t va>>(12+2*9)
$63 = 11111111
(gdb) p/x va>>30
$117 = 0xff
(gdb) p/d va>>30&0x1ff
$79 = 255

(gdb) p pagetable[255]
$11 = 0
(gdb) p &pagetable[255]
$13 = (uint64 *) 0x87f6e7f8
(gdb) p/x pagetable + 255
$67 = 0x87f6c7f8

// define PX macro in gdb: 
// define px
//   set $level = $arg0
//   set $va = $arg1
//   set $res = ($va >> (12 + 9 * $level)) & 0x1ff
//   p/x $res
// end

// px 2 0x3 ffffff 000

// “From this page table page, pick the entry that corresponds to the va bits for this level.”
// PX(level, va): compute the index by extracting the right 9 bits for that level  ->  index in that level page table (0–511).
// pagetable[PX(level, va)]: get the entry of that level page table at index
// &pagetable[PX(level, va)]: Take the address of that entry, so pte is a pointer to the entry (PTE) inside that page.
pte_t *pte = &pagetable[PX(level, va)]; 

// In xv6 (RISC-V), a page table page is just an array of 512 entries:
(gdb) p/x pagetable[255]
$12 = 0x0 //That means the 256th PTE (index 255) has the value 0 — i.e., no mapping yet.

// &pagetable[255] is the address in physical memory where that entry lives.
// &pagetable[255] = 0x87f6e000 + 255 * 8
//                  = 0x87f6e000 + 0x7f8
//                  = 0x87f6e7f8
// So 0x87f6e7f8 is simply the memory address of that entry.
// Its contents are 0 (no PTE yet), but the location is real.

(gdb) p pte
$15 = (pte_t *) 0x87f6e7f8

(gdb) p *pte
$16 = 0

// allocate a new page-table page by kalloc
// this is the page table page for next level=1 page table
// if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)  
(gdb) p pagetable
$16 = (pagetable_t) 0x87f6d000

//At each non-leaf level of the page table walk, the PTE does not point to real user memory.
// Instead, the PTE points to the physical address of the next-level page table page.
// Converts a physical address of a page (e.g., 0x87f6e000) into the bit-shifted form that belongs in a PTE.
// - Physical addresses are shifted (>> 12 then << 10) because:
// - The bottom 12 bits are always 0 (4KB aligned).
// - PTE stores the physical page number (PPN) starting at bit 10.
// Fill the current PTE with the physical address of the new page table (PA2PTE(pagetable)) and mark valid (PTE_V).
// This is how xv6 creates new page tables on demand.

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte) (((pte) >> 10) << 12)

(gdb) p/x ((uint64)pagetable)>>12
$21 = 0x87f6d
(gdb) p/x ((((uint64)pagetable)>>12)<<10)
$24 = 0x21fdb400

// Fill the current PTE with the physical address of the new page table (PA2PTE(pagetable)) and mark valid (PTE_V).
// when we want to access via virtual address:
// va -> index of level 2 -> pte -> physical address number of next level page table(4KB = 512 entries * 8 Bytes)
*pte = PA2PTE(pagetable) | PTE_V; 

(gdb)p pte
0x21fdb401

// For level 1 
(gdb) p level
$26 = 1
// the new page table for level 1 from kalloc
(gdb) p pagetable
$27 = (pagetable_t) 0x87f6d000

// va-> index of level 1: get the level 1 page table index from va
(gdb) p/x va
$35 = 0x3ffffff000
(gdb) p/x va>>(12+1*9)
$32 = 0x1ffff 
(gdb) p/t va>>(12+1*9)
$65 = 11111111111111111
(gdb) p/d va>>21&0x1ff
$84 = 511

(gdb) p/x pagetable[511]
$17 = 0x0
(gdb) p/x &pagetable[511]
$20 = 0x87f6dff8

// the pte for level 1
// pte_t *pte = &pagetable[PX(level, va)];  
(gdb) p pte
$18 = (pte_t *) 0x87f6dff8
(gdb) p *pte
$19 = 0  // no mapping yet

// create new page table for level 0 by kalloc
// if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
(gdb) p pagetable
$21 = (pagetable_t) 0x87f6c000

// pte points to new page table phsyical address num
//  *pte = PA2PTE(pagetable) | PTE_V; 


// For level 0 
// return &pagetable[PX(0, va)];
(gdb) p pagetable
$23 = (pagetable_t) 0x87f6c000

(gdb) p/x va>>12
$33 = 0x3ffffff
(gdb) p/t va>>12
$66 = 11111111111111111111111111
(gdb) p/d (va>>12)&0x1ff
$85 = 511

(gdb) p/x pagetable +511
$70 = 0x87f6cff8
(gdb)  p &pagetable[511]
$37 = (uint64 *) 0x87f6dff8
(gdb)  p pagetable[511]
$36 = 0

// finish walk() back to uvmcreate()
(gdb) p pte
$28 = (pte_t *) 0x87f6dff8
(gdb) p *pte
$26 = 0 // not mapping yet

```

### Ex: Get physical address of trampoline page 
trampoline page va= 0x3ffffff000 in Sv39, 
a pagetable start at 0x87f6e000
I have create a pagetable mapping from va= 0x3ffffff000 to pa= 0x8000a000
Now I want to get the physical addresss at va=0x3ffffff000 with this pagetabel, 
```c

// 0. root at  pagetable
(gdb) p pagetable
$125 = (pagetable_t) 0x87f6e000

(gdb) p/x va
$31 = 0x3ffffff00

// 1. extract VPN indices: VPN2 = (va >> 30) & 0x1FF, VPN1 = (va >> 21) & 0x1FF, VPN0 = (va >> 12) & 0x1FF.
// Page offset = 0x000 (low 12 bits)
// VPN[2] = bits [38:30] = 0x0FF = 255
// VPN[1] = bits [29:21] = 0x1FF = 511
// VPN[0] = bits [20:12] = 0x1FF = 511
(gdb) p/x (va>>30) & 0x1ff
$139 = 0xff
(gdb) p/x (va>>21) & 0x1ff
$140 = 0x1ff
(gdb) p/x (va>>12) & 0x1ff
$141 = 0x1ff

//2. compute address of PTE at each level: pte_addr = pagetable_base + index * 8.
// (pointer arithmetic: +index advances by 8 * index bytes)

//2.1  level-2 PTE: address and contents
// Use VPN[2]=255 → pick entry 255 in root pagetable (0x87f6cff8).
// the pte2 address
(gdb) p &pagetable[255]
$110 = (uint64 *) 0x87f6e7f8
(gdb) p/x 0x87f6e000 + 255*8
$118 = 0x87f6e7f8

//  show 8-byte PTE value: x/gx <addr> prints 8 bytes at <addr> in hex (a full PTE).
(gdb) x/gx 0x87f6e7f8
0x87f6e7f8:     0x0000000021fdb401

// the pte2 value
(gdb) p/x pagetable[255]
$145 = 0x21fdb401


// 2.2 examine that PTE: valid? leaf?
// Check PTE_V (bit 0) — if not set, the mapping is invalid.
// Check whether the PTE is a non-leaf: If (pte & (PTE_R|PTE_W|PTE_X)) == 0 it’s non-leaf, then it points to next-level page table (not to a physical data page). 
// next pagetable base = PTE2PA(pte) = ((pte >> 10) << 12)
// typedef uint64 pte_t;  
// uint64 is really unsigned long long for the compiler, n GDB you can just do: unsigned long long
(gdb) set $pte2 = pagetable[255]
// same as: 
(gdb) set $pte2 = *(unsigned long long *)&pagetable[255]
// pagetable[255]: get the value fo index 255 in pagetable array
// &pagetable[255]: get the address of index 255 in pagetable array
// (unsigned long long *)&pagetable[255]: convert to  unsigned long long * pointer
// *(unsigned long long *)&pagetable[255]: get the value of the pointer
(gdb) p/x $pte2
$149 = 0x21fdb401


(gdb) p/t $pte2
$150 = 100001111111011011010000000001

// PTE_V (valid) bit
(gdb) p/x $pte2 & 0x1
$134 = 0x21fdb401
//  check leaf bits: PTE_R|PTE_W|PTE_X (R/W/X are bits in pte): 
(gdb) p/x $pte2 & ((1<<1)|(1<<2)|(1<<3))
$148 = 0x0

// 2.3 compute next-level pagetable physical base (if non-leaf)
// Use VPN[1]=511 → pick entry 511 there.
// pagetable1 = (pte_t *)PTE2PA(*pte2);
// pte_t *pte1 = &pagetable1[511];
(gdb) p/x ($pte2>>10)<<12
$135 = 0x87f6d000 //the new page table for level 1 from kalloc

(gdb) set $next_pt = (($pte2 >> 10) << 12)
(gdb) p/x $next_pt
$155 = 0x87f6d000

// 3. Level-1: same steps (use $next_pt as base)
// 3.1 use $next_pt as base to get the pte1_addr, 
// convert to a unsigned long long* pointer and use VPN2 index
(gdb) p/d (va>>21) & 0x1ff
$174 = 511

// get the pte1_addr from level 1 page table
(gdb) p/x &((unsigned long long*)$next_pt)[511]
$166 = 0x87f6dff8

(gdb) set $pte1_addr = &((unsigned long long*)$next_pt)[511]
(gdb) p/x $pte1_addr
$167 = 0x87f6dff8

// get the pte1 value  
// or by : 
// (gdb) p/x ((unsigned long long*)$next_pt)[511]
// (gdb) set $pte1 = ((unsigned long long*)$next_pt)[511]
(gdb) set $pte1 = *$pte1_addr
(gdb) p/x $pte1
$169 = 0x21fdb001


// 3.2 checke leaf bits: valid? leaf?
(gdb) p/x $pte1 & 0x1
$170 = 0x1
(gdb) p/x $pte1 & ((1<<1)|(1<<2)|(1<<3))
$171 = 0x0

// 3.3 compute next-level pagetable physical base (if non-leaf)
(gdb) p/x ($pte1>>10)<<12
$172 = 0x87f6c000 // the new page table physical base for level 0 by kalloc
(gdb) set $next_pt = (($pte1 >> 10) << 12)
(gdb) p/x $next_pt
$173 = 0x87f6c000

// 4. Level-0: final PTE and physical page base
// 4.1 use $next_pt as base to get the pte0_addr,
// Use VPN[0]=511 → pick entry 511 there.
// That entry contains PPN of the real physical page.
(gdb) p/d (va>>12) & 0x1ff
$175 = 511
// get the pte0_addr
(gdb) p/x &((unsigned long long*)$next_pt)[511]
$188 = 0x87f6cff8
(gdb) set $pte0_addr = &((unsigned long long*)$next_pt)[511]
(gdb) p/x $pte0_addr
$189 = 0x87f6cff8
// get the pte0 value
// or by: set $pte0 = ((unsigned long long*)$next_pt)[511]
(gdb) set $pte0 = *$pte0_addr
(gdb) p/x $pte0
$191 = 0x2000280b  // the final PTE(shifted physical address + flags)

// 4.2 checke leaf bits: valid? leaf?
(gdb) p/x $pte0 & 0x1
$192 = 0x1
(gdb) p/x $pte0 & ((1<<1)|(1<<2)|(1<<3))
$193 = 0xa

(gdb) set $phys_page_base = (($pte0 >> 10) << 12)
(gdb) p/x $phys_page_base
$194 = 0x8000a000

// 5. Construct final physical address = phys_page_base | (va & 0xfff)
// Concatenate that PPN with offset
(gdb) set $phys = $phys_page_base | (va & 0xfff)
(gdb) p/x $phys
$51 = 0x8000a000
```



## 3. Load program segments： code and data

```C
for each program header ph:
   if(ph.type == ELF_PROG_LOAD) {
      uvmalloc(..., ph.vaddr+ph.memsz);
      loadseg(..., ph.vaddr, file, ph.off, ph.filesz);
   }

```
- `exec()` reads the ELF binary.
- Iterates over program headers (the ELF file says what sections should be loaded).
  - `uvmalloc` (kernel/exec.c:52) allocates physical memory for each ELF segment : heap + code + data
  - `loadseg` (kernel/exec.c:10)  loads each segment into the allocated memory
  `loadseg` uses `walkaddr` to find the physical address of the allocated memory at which to write each page of the ELF segment, and readi to read from the file.
This builds the **text, data, and bss regions** at the lowest part of the address space.
At this point, code and data of the program are in the new pagetable.

Exec loads bytes from the ELF file into memory at addresses specified by the ELF file. 
Users or processes can place whatever addresses they want into an ELF file. Thus exec is risky, because the addresses in the ELF file may refer to the kernel, accidentally or on purpose. 
The consequences for an unwary kernel could range from a crash to a malicious subversion of the kernel’s isolation mechanisms (i.e., a security exploit). 

Xv6 performs a number of checks to avoid these risks. 
For example `if(ph.vaddr + ph.memsz < ph.vaddr)` checks for whether the sum overflows a 64-bit integer. 
The danger is that a user could construct an ELF binary with a ph.vaddr that points to a user-chosen address, and ph.memsz large enough that the sum overflows to 0x1000,
which will look like a valid value. 

In an older version of xv6 in which the user address space also contained the kernel (but not readable/writable in user mode), the user could choose an address that corresponded to kernel memory and would thus copy data from the ELF binary into the kernel. 
In the RISC-V version of xv6 this cannot happen, because the kernel has its own separate page table;

loadseg loads into the process’s page table, not in the kernel’s page table.

It is easy for a kernel developer to omit a crucial check, and real-world kernels have a long history of missing checks whose absence can be exploited by user programs to obtain kernel privileges.
It is likely that xv6 doesn’t do a complete job of validating user-level data supplied to the kernel, which a malicious user program might be able to exploit to circumvent xv6’s isolation.

### Debug `uvmalloc`

Why is this needed in exec()?
Because exec is loading an ELF program. Each ELF segment says:
- load this file content at virtual address `ph.vaddr`
- allocate memory up to `ph.vaddr + ph.memsz`
So uvmalloc() ensures those addresses actually exist in the process page table and point to fresh physical pages.
Later, exec() will copy the ELF file’s contents into those physical pages.

uvmalloc() is the memory grower: creates user PTEs + allocates physical pages.
exec() uses it to make space for ELF segments.
After uvmalloc, the new process has valid memory, so exec can safely readi() ELF contents into it.

```c
// if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0) 

// pagetable → new page table being built for the process (empty + trampoline + trapframe already set up).
// sz → current allocated size in the process (starts from 0 for new exec).
// ph.vaddr + ph.memsz → end of this ELF segment (virtual address + size of segment).
// This is the "target size" the process needs.
// "grow the process’s address space from sz up to ph.vaddr + ph.memsz."
(gdb) p pagetable
$3 = (pagetable_t) 0x87f6e000
(gdb) p/x pagetable[255]&0x3ff
$8 = 0x1

(gdb) p sz
$22 = 0
(gdb) p ph.vaddr
$20 = 0
(gdb) p ph.memsz
$21 = 4056


// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}


(gdb) p oldsz
$12 = 0
// The new size of process’s memory.
(gdb) p newsz
$13 = 4056

// mem = kalloc();
// allocate one physical 4KB page.
(gdb) p mem
$15 = 0x87f6b000 '\005' <repeats 200 times>...

// memset(mem, 0, PGSIZE); 
// clear it (so process doesn’t see garbage memory).
(gdb) p mem
$16 = 0x87f6b000 ""

// mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U);
// Insert into the process’s page table:
// Virtual address a → Physical address mem.
// Permissions: readable, writable, executable, user.

```

### Debug `loadseg`

In xv6, all user programs are mapped starting at virtual address 0x0.

so that when we debug the user prgrom at Kernel GDB, we can use :
(gdb) add-symbol-file user/_sh 0x0
               

```c
//Copy segment contents into the allocated memory using loadseg()
if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0
// ph.vaddr comes from the ELF program header.
// In xv6 user binaries, ph.vaddr = 0x0.
// So ELF text segment is mapped at virtual address 0.

(gdb) p path
$10 = 0x3fffff9f08 "echo"

(gdb) p ph.vaddr
$4 = 0
(gdb) p/x ph.vaddr
$5 = 0x0
(gdb) p ip
$6 = (struct inode *) 0x80023390 <icache+296>
(gdb) p ph.off
$7 = 176
(gdb) p ph.filesz
$8 = 3801
(gdb) p pagetable
$9 = (pagetable_t) 0x87f47000


// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  if((va % PGSIZE) != 0)
    panic("loadseg: va must be page aligned");

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }
  
  return 0;
}c
```


```bash
$ objdump -h user/_echo

user/_echo:     file format elf64-little

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 .text         00000e88  0000000000000000  0000000000000000  000000b0  2**1
                  CONTENTS, ALLOC, LOAD, CODE
  1 .rodata       0000003c  0000000000000e88  0000000000000e88  00000f38  2**3
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 .data         00000011  0000000000000ec8  0000000000000ec8  00000f78  2**3
                  CONTENTS, ALLOC, LOAD, DATA
  3 .bss          00000018  0000000000000ee0  0000000000000ee0  00000f89  2**3
                  ALLOC
  4 .comment      00000029  0000000000000000  0000000000000000  00000f89  2**0
                  CONTENTS, READONLY
  5 .debug_aranges 00000120  0000000000000000  0000000000000000  00000fc0  2**4
                  CONTENTS, READONLY, DEBUGGING, OCTETS
  6 .debug_info   00000b17  0000000000000000  0000000000000000  000010e0  2**0
                  CONTENTS, READONLY, DEBUGGING, OCTETS
  7 .debug_abbrev 00000449  0000000000000000  0000000000000000  00001bf7  2**0
                  CONTENTS, READONLY, DEBUGGING, OCTETS
  8 .debug_line   00000e72  0000000000000000  0000000000000000  00002040  2**0
                  CONTENTS, READONLY, DEBUGGING, OCTETS
  9 .debug_frame  00000468  0000000000000000  0000000000000000  00002eb8  2**3
                  CONTENTS, READONLY, DEBUGGING, OCTETS
 10 .debug_str    000002a6  0000000000000000  0000000000000000  00003320  2**0
                  CONTENTS, READONLY, DEBUGGING, OCTETS


Here the VMA = 0x00000000, which means .text is linked to start at address 0.
- .text  The section name (code section)
- VMA (virtual memory address) where the section expects to be loaded
- LMA (load memory address, usually same as VMA here)
```  


## 4. Setup the user stack

```c
// Allocate two pages at the next page boundary.
// Use the second as the user stack.
// Right below the stack, xv6 reserves a guard page (mapped invalid, so any access will cause a fault).
sz = PGROUNDUP(sz);
uint64 sz1;
//uvmalloc() is called again to allocate 2 pages for the user stack and guard page.
if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE)) == 0) 
  goto bad;
sz = sz1;
uvmclear(pagetable, sz-2*PGSIZE);
sp = sz;
stackbase = sp - PGSIZE;
```

- Actual user stack.
Now exec allocates and initializes the user stack. It allocates just `one stack page`. 
Exec copies the argument strings to the top of the stack one at a time, recording the pointers to them in `ustack`.
It places a null pointer at the end of what will be the argv list passed to main. 
The first three entries in ustack are the fake return program counter, argc, and argv pointer.

- Guard page (inaccessible).
Exec places an inaccessible page just below the stack page, so that programs that try to use more than one page will fault. 
This inaccessible page also allows exec to deal with arguments that are too large; 
in that situation, the `copyout` (kernel/vm.c:347) function that exec uses to copy arguments to the stack will notice that the destination page is not accessible, and will return -1.

```c
(gdb) p sz
$28 = 4056
(gdb) p sz1
$29 = 12288
(gdb) p sz+2*4096
$30 = 12288
// sz = sz1;
// uvmclear(pagetable, sz-2*PGSIZE);
(gdb) p sz
$31 = 12288

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

(gdb) p pagetable
$32 = (pagetable_t) 0x87f6e000
(gdb) p va
$33 = 4096

// pte = walk(pagetable, va, 0);
(gdb) p pte
$39 = (pte_t *) 0x87f69008

(gdb) p/t 1L<<4
$41 = 10000
(gdb) p/t ~(1L<<4)
$42 = 1111111111111111111111111111111111111111111111111111111111101111

(gdb) p/t *pte
$44 = 100001111111011010000000011111
// change the PTE_U bit
(gdb) p/t *pte & ~(1L<<4)
$45 = 100001111111011010000000001111

```

During the preparation of the new memory image, if exec detects an error like an invalid program segment, it jumps to the label bad, frees the new image, and returns -1. 

Exec must wait to free the old image until it is sure that the system call will succeed: 
if the old image is gone, the system call cannot return -1 to it. The only error cases in exec happen during the creation of the image. 
Once the image is complete, exec can commit to the new page table (kernel/exec.c:113) and free the old one (kernel/exec.c:117).

## 5. Copy arguments onto the stack

At this point in exec(), the process’s new memory (code, data, bss) is already set up.
this is the final stage of exec(): setting up the user stack with the arguments (argv[]) before jumping into the new program
Now xv6 needs to make the stack look like what main(int argc, char *argv[]) expects.
- argv (kernel side) → the arguments passed to exec(path, argv) (strings in kernel memory).
- ustack[] (kernel array of uint64) → temporary storage for the future argv[] pointers inside user space.

```C
// Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;
```

Pushes each string argument (argv[i]) into the user stack memory.
Records their addresses in ustack[].
Then pushes the whole argv[] array itself onto the stack:

### Debuging argc
```c
// sp = current top of user stack (starts high, grows downward).
(gdb) p/d sp
$2 = 12288
(gdb) p/x sp
$47 = 0x3000
(gdb) p/x stackbase
$48 = 0x2000

// sp -= strlen(argv[argc]) + 1;
// move stack pointer down by string length
(gdb) p sp
$6 = 12282

//  sp -= sp % 16; // riscv sp must be 16-byte aligned  
(gdb) p sp
$10 = 12272

(gdb) p argc
$3 = 0
// argv is a user-space pointer, not directly readable from kernel GDB, since GDB is debugging the kernel (and kernel can’t just deref user memory).
(gdb) p argv
$4 = (char **) 0x3fffffde08

// Use x/s on the user-space addresses after copyout has run:
// (gdb) x/4s 0x3fffffde08
// This will try to read 4 strings starting at that user-space pointer.


// if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)  
// Each argv[i] string is copied into user memory using copyout.


```

### Debug copyout

```c
// if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
(gdb) p pagetable
$20 = (pagetable_t) 0x87f6e000
(gdb) p dstva
$21 = 12256
(gdb) p src
$22 = 0x3fffffdc58 "\360/"
(gdb) p len
$23 = 16

(gdb) p va0
$24 = 8192
(gdb) p dstva/4096
$26 = 2
(gdb) p dstva%4096
$27 = 4064

// pa0 = walkaddr(pagetable, va0);
(gdb) p/x pa0
$29 = 0x87f67000

(gdb) p dstva -va0
$31 = 4064

(gdb) p n
$32 = 32
```


## 6. Map trapframe (initial CPU state)

```c
p->trapframe->a1 = sp;      // argv pointer
p->trapframe->epc = elf.entry; // program entry point (main)
p->trapframe->sp = sp;      // user stack pointer

(gdb) p sp
$54 = 12256
(gdb) p/x sp
$55 = 0x2fe0

```

* At the very top of user address space, xv6 maps:

  * **Trampoline page**: contains code to enter/exit the kernel on `ecall`.
  * **Trapframe page**: per-process data (registers saved during a trap).

These are special mappings so the user process can enter the kernel safely.


When process resumes in user mode:
- a0 = argc (returned from exec()).
- a1 = argv (pointer to arguments).
- epc (program counter) = entry point of ELF.
- sp = top of the user stack.

This matches the C calling convention: main(int argc, char *argv[]).


## 7. Commit new address space


* Save the newly created `pagetable` in the process structure (`p->pagetable`).
* Save the size of memory (`p->sz`), stack pointer, and program counter (`p->trapframe->epc = elf_entry`).
* Now the process has its own virtual address space.


```C
oldpagetable = p->pagetable;
p->pagetable = pagetable; // switch process to new page table
p->sz = sz;
proc_freepagetable(oldpagetable, oldsz);
```
Swaps in the new page table as the process’s address space.
Frees the old one (from the program that just got replaced).

### Debug proc_freepagetable

```c
// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}
```


## 8. Return to userspace

The process continues execution in user mode, at `elf.entry` (typically `_start` in crt0.S), with a fresh address space and stack.

* When scheduled, the CPU switches to this new process’s page table.
* The process begins executing at the ELF entry point (usually `_start`).





# Debug a user process: echo

### Boot xv6 and debug runcmd
```c
// after xv6 is booted
// xv6 kernel is booting
// init: starting sh
$ echo hi

// Kernel GDB session only knows kernel structs.
// To inspect cmd (a user-space struct cmd from sh.c), you must load the user/_sh ELF symbols with add-symbol-file.

(gdb) add-symbol-file user/_sh 0x0
add symbol table from file "user/_sh" at
        .text_addr = 0x0
Reading symbols from user/_sh...

// Meaning: “Load the debug symbols for user/_sh, and tell GDB that the code starts at virtual address 0x0.”
// user/_sh is the ELF for the shell program (compiled with symbols).
// 0x0 is its load base (xv6 loads user programs at VA 0 by kernel)
// After this, GDB will know how to map struct cmd, main(), etc. to the addresses you see in your cmd pointer.

(gdb) b runcmd
(gdb) c


//  the memory map is user space
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

// cmd is a pointer to a struct cmd
(gdb) p cmd
$2 = (struct cmd *) 0x13f50
(gdb) info reg sp
sp             0x3f70   0x3f70

(gdb) p *cmd
$12 = {
  type = 1
}

// case EXEC:
//   ecmd = (struct execcmd*)cmd;
//   if(ecmd->argv[0] == 0)
//     exit(1);


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



//   exec(ecmd->argv[0], ecmd->argv);
//   fprintf(2, "exec %s failed\n", ecmd->argv[0]);
//   break;

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

```

### Debug echo in exec
```c
(gdb) b exec

// after trap and load satp register to kenerl mememory space
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
0000000080000000 0000000080000000 000000000000a000 r-x--a-
000000008000a000 000000008000a000 0000000000001000 r-x----
000000008000b000 000000008000b000 0000000000002000 rw---ad
000000008000d000 000000008000d000 0000000000007000 rw-----
0000000080014000 0000000080014000 0000000000011000 rw---ad
0000000080025000 0000000080025000 0000000000001000 rw-----
0000000080026000 0000000080026000 0000000000003000 rw---ad
0000000080029000 0000000080029000 0000000007f17000 rw-----
0000000087f40000 0000000087f40000 0000000000078000 rw---ad
0000000087fb8000 0000000087fb8000 0000000000001000 rw---a-
0000000087fb9000 0000000087fb9000 0000000000046000 rw-----
0000000087fff000 0000000087fff000 0000000000001000 rw---a-
0000003ffff7f000 0000000087f77000 000000000003d000 rw-----
0000003fffff9000 0000000087fb4000 0000000000003000 rw---ad
0000003ffffff000 000000008000a000 0000000000001000 r-x--a-


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
#0  0x0000000080002c54 in proc_pagetable (p=0x80015048 <proc+752>) at kernel/proc.c:202
#1  0x00000000800074b8 in exec (path=0x3fffff9f08 "echo", argv=0x3fffff9e08) at kernel/exec.c:41
#2  0x0000000080008838 in sys_exec () at kernel/sysfile.c:447
#3  0x0000000080004402 in syscall () at kernel/syscall.c:175
#4  0x0000000080003d26 in usertrap () at kernel/trap.c:67
#5  0x000000000000008c in runcmd (cmd=<error reading variable: Cannot access memory at address 0x3f78>) at user/sh.c:78
Backtrace stopped: previous frame inner to this frame (corrupt stack?)

```
# ELF file
## What is ELF file?
An **ELF file** (Executable and Linkable Format) is a **standard binary file format** used by Unix-like operating systems (including Linux, xv6, BSD, etc.) for executables, object code, shared libraries, and core dumps.

Think of it as a *container format* that organizes everything the OS and loader need to run a program.

---

### Structure of an ELF file

An ELF file has several parts:

1. **ELF Header (`struct elfhdr`)**

   * Identifies the file as ELF.
   * Contains metadata like:

     * File type (executable, relocatable, shared lib…)
     * Target architecture (e.g., RISC-V in xv6, x86 in Linux)
     * Entry point address (where execution starts → usually `_start` or `main`)
     * Offset to the program header table
     * Offset to the section header table

2. **Program Header Table (`struct proghdr[]`)**

   * Tells the loader **what parts of the file to load into memory**.
   * Each entry describes a segment:

     * `ph.vaddr` → virtual address to load at
     * `ph.off` → offset in the file
     * `ph.filesz` → size in file
     * `ph.memsz` → size in memory (can be larger, for uninitialized data like `.bss`)
   * `type = ELF_PROG_LOAD` means “load this segment into memory”.

3. **Sections / Section Header Table**

   * Used mostly for linking, not loading.
   * Contains things like symbol tables (`.symtab`), string tables (`.strtab`), debugging info.
   * Not even needed at runtime in stripped executables.

---

### How xv6 uses ELF in `exec()`

In your `exec()` code:

1. It opens the ELF file from the filesystem.
2. Reads the **ELF header** (`struct elfhdr`) → checks `elf.magic == ELF_MAGIC`.
3. Iterates through the **program headers** (`struct proghdr`).

   * For each loadable segment (`ELF_PROG_LOAD`):

     * Allocates memory (`uvmalloc`).
     * Loads data from file into memory (`loadseg`).
4. Sets up stack pages, arguments, and trapframe.
5. Sets the new process’s program counter (`epc = elf.entry`) to the **entry point** specified in the ELF header.

So the ELF file tells xv6 **how to map the binary into memory** and **where to start execution**.

---

**In short:**
An ELF file is the standard way to package executables.
It contains both instructions (code), data, and metadata about where/how they should be loaded into memory.
xv6’s `exec()` acts as a *minimal ELF loader*.

# 3.9 Real world


