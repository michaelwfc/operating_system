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

## Teams

### Physical memory
- Physical memory refers to storage cells in DRAM. 
- A byte of physical memory has an address, called a `physical address`. 
- Instructions use only virtual addresses, which the paging hardware translates to physical addresses, and then sends to the DRAM hardware to read or write storage. 
- Unlike physical memory and virtual addresses, virtual memory isn’t a physical object, but refers to the collection of abstractions and mechanisms the kernel provides to manage physical memory and virtual addresses.


### Virtual address
Virtual address = [Virtual Page Number | Page Offset]

- `Virtual Page Number` (high bits):  Only the Virtual page number is looked up in the page table.
- `Offset` (low bits,12 bits → 0–4095): The offset is copied unchanged → this makes translations efficient

### Page table
- A page table is a data structure in memory used by the OS + hardware Memory Management Unit(MMU).
- It defines, for each virtual page number (VPN), which physical page number (PPN) it maps to.
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
- only the bottom `39 bits` of a 64-bit virtual address are used; 39 bits= 27 bits for index + 12 bits for offset
- the top `25 bits` are not used. 
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

VA[38:30]  → top-level index (512 entries)
VA[29:21]  → mid-level index (512 entries)
VA[20:12]  → leaf-level index (512 entries)
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


### Flage in each PTE
Each PTE contains flag bits that tell the paging hardware how the associated virtual address
is allowed to be used.

- PTE_V indicates whether the PTE is present: if it is not set, a reference to the page causes an exception (i.e. is not allowed). 
- PTE_R controls whether instructions are allowed to read to the page. 
- PTE_W controls whether instructions are allowed to write to the page. 
- PTE_X controls whether the CPU may interpret the content of the page as instructions and execute them.
- PTE_U controls whether instructions in user mode are allowed to access the page; if PTE_U is not set, the PTE can be used only in supervisor mode.
Figure 3.2 shows how it all works. The flags and all other page hardware-related structures are defined in (kernel/riscv.h)

###  satp Register
To tell the hardware to use a page table, the kernel must write the `physical address` of `the root(top-level) page-table page` into the satp register. 
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



### How multi-level page tables work
the key idea: 
the first PPN in the top-level page diretory contains the physical page number (PPN) of the next-level down,
the final entry in the `final-level page directory` contains the actual  physical page number (PPN) of the page that we acturally tring to translat to

`An invalid entry at level N `means you don’t need to allocate the next-level page table for that entire region of the address space.

1. What does a top-level entry represent?

In Sv39, the top-level page table (root) has 512 entries.
Each entry can either:
- Be valid, meaning it points to a second-level page table (another 4KB page that contains 512 PTEs).
- Be invalid, meaning this entire range of virtual addresses has no mapping.


1. What address range does one top-level entry cover?
Each level indexes 9 bits = 512 possibilities.
Top-level index picks 512 GB chunks of the virtual address space.
Entry 0 = VA 0x0000_0000_0000 – 0x0000_7FFF_FFFF (the first 512 GB).
Entry 1 = next 512 GB.
…
Entry 511 = last 512 GB.
So each top-level entry = 512 GB virtual address range.


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





### Example: one page in Sv39

Let’s pick a virtual page boundary at 0x000000400000 (hex).
This is divisible by 0x1000 (4096), so it’s page-aligned.
That page covers:
```sql
0x000000400000   (start of page, offset 0x000)
...
0x000000400FFF   (end of page, offset 0xFFF)
```
- Virtual page number (VPN): comes from top 27 bits.
- Offset (12 bits): selects which byte inside this 4KB page.


Example virtual addresses inside that page:
- 0x000000400000 → offset = 0x000 (first byte in page)
- 0x000000400123 → offset = 0x123 (291st byte in page)
- 0x000000400FFF → offset = 0xFFF (last byte in page)

All these belong to the same page.

#### Mapping to physical page 
Suppose the page table entry (PTE) for VPN = 0x400 maps to physical page number (PPN) = 0xABCDE.

Then hardware builds physical address as:
Physical address = [PPN (44 bits) | Page offset (12 bits)]

Virtual 0x000000400123 :  VPN = 0x400, offset = 0x123
Physical = 0xABCDE0123

Virtual 0x000000400FFF: VPN = 0x400, offset = 0xFFF
Physical = 0xABCDE0FFF

Both virtual addresses map to the same physical 4KB chunk (page), just different offsets.

So "aligned chunks of 4096 bytes" = memory blocks that start at addresses multiple of 0x1000 (4096), each block exactly 4KB.
The OS can only control mappings at this page granularity — it can’t say “map only 2 bytes differently,” it must map the whole 4KB chunk.





# 3.2 Kernel address space
![image](../images/Figure%203.3-%20xv6's%20kernel%20address%20space.png)

## The Right part: Physical memory in DRAM
- ox0000: unused
- ox1000: the pyshical address of boot ROOM
  when you turn on the board, the first thing the board does is to jump to the boot ROM, run the code inthe boot ROM, and then jump to 0x8000 0000 the kernel.
- PLIC: Platform Level Interrupt Controller
- CLINT: Core Level Interrupt Controller
- UART: Universal Asynchronous Receiver/Transmitter
- VIRTIO disk: Virtual I/O Device Interface
  


Xv6 maintains `one page table per process`, describing each process’s `user address space`, plus a single
page table that describes the `kernel’s address space`. 
The kernel configures the layout of its address space to give itself access to physical memory and various hardware resources at predictable virtual addresses.

## QEMU
- QEMU simulates a computer that includes RAM (physical memory) starting at physical address `0x80000000` and continuing through at least `0x86400000`, which xv6 calls `PHYSTOP`.
- The QEMU simulation also includes I/O devices such as a disk interface. 
- QEMU exposes the device interfaces to software as memory-mapped control registers that sit below 0x80000000 in the physical address space. 
- The kernel can interact with the devices by reading/writing these special physical addresses; such reads and writes communicate with the device hardware rather than with RAM. 

Chapter 4 explains how xv6 interacts with devices.

## Direct mapping
The kernel gets at RAM and memory-mapped device registers using `“direct mapping`” that is, mapping the resources at virtual addresses that are equal to the physical address. 

For example, the kernel itself is located at `KERNBASE=0x80000000` in both the virtual address space and in
physical memory. 
Direct mapping simplifies kernel code that reads or writes physical memory. 
For example, when fork allocates user memory for the child process, the allocator returns the physical
address of that memory; fork uses that address directly as a virtual address when it is copying the
parent’s user memory to the child

## Not direct-mapped
There are a couple of kernel virtual addresses that aren’t direct-mapped:
- `The trampoline page`. 
  It is mapped at the top of the virtual address space; user page tables have this same mapping. Chapter 4 discusses the role of the trampoline page, but we see here an interesting use case of page tables; a physical page (holding the trampoline code) is `mapped twice` in the virtual address space of the kernel: once at top of the virtual address space and once with a direct mapping.


- `The kernel stack pages`. 
    Each process has its own kernel stack, which is mapped high so that below it xv6 can leave an unmapped guard page. 
    The guard page’s PTE is invalid (i.e., PTE_V is not set), so that if the kernel overflows a kernel stack, it will likely cause an exception and the kernel will panic. Without a guard page an overflowing stack would overwrite other kernel memory, resulting in incorrect operation. A panic crash is preferable.


# 3.3 Code: creating an address space

Most of the xv6 code for manipulating address spaces and page tables resides in vm.c (kernel/vm.c:1). 




## Structures & Functions

### `pagetable_t`
The central data structure is pagetable_t, which is really a pointer to a RISC-V root page-table page; a pagetable_t may be either the kernel page table, or one of the perprocess page tables. 

### `walk`
The central functions are walk, which finds the PTE for a virtual address,
and mappages, which installs PTEs for new mappings. 

### kvm
Functions starting with kvm manipulate the `kernel page table`; 

### uvm
functions starting with uvm manipulate a `user page table`; 

other functions are used for both. 

### copyout and copyin
copyout and copyin copy data to and from user virtual addresses provided as
system call arguments; they are in vm.c because they need to explicitly translate those addresses in order to find the corresponding physical memory.

## Creating an address space

### kvminit

Early in the boot sequence, main calls `kvminit` (kernel/vm.c:54) to create the kernel’s page table using `kvmmake` (kernel/vm.c:20). This call occurs before xv6 has enabled paging on the RISC-V, so addresses refer directly to physical memory. 

- `Kvmmake` first allocates a page of physical memory to hold the root page-table page. 
- Then it calls `kvmmap` to install the translations that the kernel needs. The translations include the kernel’s instructions and data, physical memory up to PHYSTOP,and memory ranges which are actually devices. - `Proc_mapstacks` (kernel/proc.c:33) allocates a kernel stack for each process. It calls kvmmap to map each stack at the virtual address generated by KSTACK, which leaves room for the invalid stack-guard pages.

### kvmmap
kvmmap (kernel/vm.c:127) calls `mappages` (kernel/vm.c:138), which installs mappings into a page table for a range of virtual addresses to a corresponding range of physical addresses. It does this separately for each virtual address in the range, at page intervals. 

For each virtual address to be mapped, mappages calls `walk` to find the address of the PTE for that address. It then initializes the PTE to hold the relevant physical page number, the desired permissions (PTE_W, PTE_X, and/or PTE_R), and PTE_V to mark the PTE as valid (kernel/vm.c:153).

### walk
walk (kernel/vm.c:81) mimics the RISC-V paging hardware as it looks up the PTE for a virtual
address (see Figure 3.2). walk descends the 3-level page table 9 bits at the time. It uses each level’s 9 bits of virtual address to find the PTE of either the next-level page table or the final page (kernel/vm.c:87). If the PTE isn’t valid, then the required page hasn’t yet been allocated; if the alloc argument is set, walk allocates a new page-table page and puts its physical address in the PTE. It returns the address of the PTE in the lowest layer in the tree (kernel/vm.c:97).


The above code depends on physical memory being direct-mapped into the kernel


```bash
# #1 terminal
$ make CPUS=1 qemu-gdb


# #2 terminal
$ gdb-multiarch kernel/kernel
(gdb) target remote :26000
(gdb) b main
(gdb) b kvminit
(gdb) c


```


# 3.4 Physical memory allocation

Each process has a separate page table, and when xv6 switches between processes, it also changes
page tables. 
As Figure 2.3 shows, a process’s user memory starts at virtual address zero and can
grow up to MAXVA (kernel/riscv.h:360), allowing a process to address in principle 256 Gigabytes of memory.

![images](../images/Figure%203.4-A%20process’s%20user%20address%20space,%20with%20its%20initial%20stack.png)

1. different processes’ page tables translate user addresses to different pages of physical memory, so that each process has private user memory. 
2. each process sees its memory as having contiguous virtual addresses starting at zero, while the process’s physical memory can be non-contiguous. 
3. the kernel maps a page with trampoline code at the top of the user address space, thus a single page of physical memory shows up in all address spaces.
 

# 3.5 Code: Physical memory allocator

The allocator resides in kalloc.c (kernel/kalloc.c:1). 
The allocator’s data structure is a free list of physical memory pages that are available for allocation. 
Each free page’s list element is a `struct run` (kernel/kalloc.c:17). 

Where does the allocator get the memory to hold that data structure?
It store each free page’s run structure in the free page itself, since there’s nothing else stored there. The free list is protected by a spin lock (kernel/kalloc.c:21-24). The list and the lock are wrapped in a struct to make clear that the lock protects the fields in the struct. For now, ignore the lock and the calls to acquire and release; Chapter 6 will examine locking in detail.


# 3.6 Process address space

# 3.7 Code: sbrk


# 3.8 Code: exec

Exec is the system call that creates the user part of an address space. It initializes the user part of an address space from a file stored in the file system. 
- Exec (kernel/exec.c:13) opens the named binary path using `namei` (kernel/exec.c:26), which is explained in Chapter 8. 
- Then, it reads the ELF header. Xv6 applications are described in the widely-used ELF format, defined in (kernel/elf.h). An ELF binary consists of an ELF header, `struct elfhdr` (kernel/elf.h:6), followed by a sequence of program section headers, `struct proghdr` (kernel/elf.h:25). 
- Each proghdr describes a section of the application that must be loaded into memory; xv6 programs have only one program section header, but other systems might have separate sections for instructions and data.



# 3.9 Real world
