# Preparation: 

Read Chapter 3 and kernel/memlayout.h, kernel/vm.c, kernel/kalloc.c, kernel/riscv.h, and kernel/exec.c

## 3.1 Paging hardware
The RISC-V page table hardware connects these two kinds of addresses, by mapping each virtual address to a physical address.

![image](../images/Figure%203.1-RISC-V%20virtual%20and%20physical%20addresses,%20with%20a%20simplified%20logical%20page%20table.png)
xv6 runs on Sv39 RISC-V, which means that 
- only the bottom 39 bits of a 64-bit virtual address are used; 
- the top 25 bits are not used. 
- In this Sv39 configuration, a RISC-V page table is logically an array of 2^27 (134,217,728) `page table entries (PTEs)`.
- Each PTE contains a 44-bit `physical page number (PPN)` and some `flags`.
- The paging hardware translates a virtual address by using the top 27 bits of the 39 bits to index into the page table to find a PTE
- making a 56-bit physical address whose top 44 bits come from the PPN in the PTE and whose bottom 12 bits are copied from the original virtual address.
- A page table gives the operating system control over virtual-to-physical address translations at the granularity of aligned chunks of 4096 (2^12) bytes. Such a chunk is called a `page`.

### RISC-V address translation details
![image](../images/Figure%203.2-RISC-V%20address%20translation%20details.png)
As Figure 3.2 shows, a RISC-V CPU translates a virtual address into a physical in three steps.

A page table is stored in physical memory as a three-level tree. 
The root of the tree is a 4096-byte page-table page that contains 512 PTEs, which contain the physical addresses for page-table pages in the next level of the tree. 
Each of those pages contains 512 PTEs for the final level in the tree.
- The paging hardware uses the top 9 bits of the 27 bits to select a PTE in the root page-table page,
- the middle 9 bits to select a PTE in a page-table page in the next level of the tree, 
- the bottom 9 bits to select the final PTE. 

(In Sv48 RISC-V a page table has four levels, and bits 39 through 47 of a virtual address index into the top-level.)
