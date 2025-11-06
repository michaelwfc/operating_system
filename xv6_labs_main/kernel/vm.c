#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"   // Must come before proc.h
#include "proc.h" 




/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Initialize the one kernel_pagetable
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

/*
 * create a direct-map page table for the kernel.
 */
pagetable_t
kvmmake(void)
{

  pagetable_t kpgtbl = (pagetable_t) kalloc();  // allocate a physical page for the top-level page directory

  memset(kpgtbl, 0, PGSIZE);   // zero it out, so that all the PTEs are zero.

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0,  VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(kpgtbl, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl,  PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl,  KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl,  (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl,  TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // map kernel stacks
  // proc_mapstacks(kernel_pagetable);
  return kpgtbl;
}




// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable that corresponds to virtual address va.  
// If alloc!=0, create any required page-table pages.
//
// Page table page size = 4 KB = 512 entries × 8 bytes.
// The risc-v Sv39 scheme has 3 levels of page-table pages. 
// Each level index is 9 bits → selects an entry from a page table (which has 512 entries).
// A page-table page contains 512 PTEs.
// Each entry is a pte_t (64-bit).

// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.

/**
pagetable: the top-level (L2) page table.
va: the virtual address we want to translate.
alloc: if true, allocate new intermediate page tables as needed.

walk() starts at the top-level page table.
For each level (L2 → L1):
- Follow existing PTE if valid.
- Otherwise, allocate a new page table (if alloc != 0).
Finally return the leaf PTE pointer (level-0 entry).
This is used both for looking up existing mappings and for creating new ones.

In RISC-V Sv39, a multi-level page table works like a tree:
Each page table page contains 512 entries (pte_t, 8 bytes each, 512 * 8 = 4096 = PGSIZE).
Each entry (PTE) can either:
- Point to the next-level page table (if PTE_V is set but not a leaf mapping), or
- Be a leaf entry that directly maps a virtual page to a physical page.
So when we’re in walk(), we might be looking at a level-2 or level-1 PTE.
If that PTE is valid, it points to the next-level page table’s physical address.

*/
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)]; 
    if(*pte & PTE_V) {
      //already valid: follow the pointer to next-level page table
      //PTE2PA(*pte):   extracts the physical address stored in the PTE.
      //(pagetable_t)PTE2PA(*pte): Cast it to pagetable_t, so we can treat that physical page as another page table.
      pagetable = (pagetable_t)PTE2PA(*pte); 
    } else {
      // not valid: allocate a new page-table page
      // If alloc == 0, we can’t proceed → return 0 (failure).
      // Otherwise, allocate a new empty page table page with kalloc().
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V; 
    }
  }
  // After the loop, we’re at level 0 (leaf).
  // Return the pointer to the leaf PTE that corresponds to va.
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// ========= solution for pgtbl ---- part 3 =============================
// copy PTEs from the user page table into this proc's kernel page table
void
kvmmapuser(int pid, pagetable_t kpagetable, pagetable_t upagetable, uint64 newsz, uint64 oldsz)
{
  uint64 va;
  pte_t *upte;
  pte_t *kpte;

  if(newsz >= PLIC)
    panic("kvmmapuser: newsz too large");

  for (va = oldsz; va < newsz; va += PGSIZE) {
    upte = walk(upagetable, va, 0);
    kpte = walk(kpagetable, va, 1);
    *kpte = *upte;
    // because the user mapping in kernel page table is only used for copyin 
    // so the kernel don't need to have the W,X,U bit turned on
    *kpte &= ~(PTE_U|PTE_W|PTE_X);
  }
}


/**
在每个进程的 内核页表（kpagetable） 中，为用户虚拟地址范围建立映射（映射到同样的物理页），
但不要设置 PTE_U（因为设置 PTE_U 的页在内核态不可访问）。
这样内核函数 copyin_new/copyinstr_new 可以直接在内核地址空间通过内核页表去解引用用户指针，
从而避免逐字节 software walk。

it mirrors all user memory mappings into that process’s kernel page table, but with kernel-only permissions.

for each user virtual address in the range [oldsz, newsz), it adds a corresponding entry to the kernel page table that points to the same physical page.

So, after calling u2kvmcopy():
- The user and kernel page tables share the same physical pages for user memory.
- The kernel can directly dereference user virtual addresses (since it has a kernel-side mapping).
- But kernel-mode access is safe, because the kernel copy of the PTE removes the PTE_U bit
*/
void u2kvmcopy(pagetable_t upgtbl, pagetable_t kpgtbl, uint64 oldsz, uint64 newsz) {
  // oldsz: the previous size of the process’s memory (in bytes).
  // newsz: the new size of the process’s memory (after growth or during creation).
  // We want to map only the newly allocated pages into the kernel’s page table — not re-copy all the old ones again.
  
  uint64 va,pa,a;
  uint flags;
  pte_t *upte,*kpte;

  va = PGROUNDUP(oldsz);
  // iterate page by page from oldsz (rounded up) to newsz.
  for(a= va;a<newsz;a+=PGSIZE){
    //Look up the corresponding user PTE
    if((upte= walk(upgtbl, a, 0)) ==0){
      panic("u2kvmcopy: user pte should exist");
    }
    // 为 kernel page table 分配页表页（如果需要）
    // walk() with alloc=1 creates intermediate page table levels as needed,
    // so that we can safely assign a leaf PTE at this address in the kernel’s page table.
    if((kpte=walk(kpgtbl,a,1)) ==0){
      panic("u2kvmcopy: kernel pte walk failed");
    }
    pa = PTE2PA(*upte);
    flags =  (PTE_FLAGS(*upte) & (~PTE_U)) | PTE_V; // remove PTE_U flag,确保内核访问权限
    //write the new PTE into the kernel’s page table.
    //Now both tables point to the same physical memory, but with slightly different access
    *kpte = PA2PTE(pa) | flags; 
  }
}



// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  struct proc *p = myproc();
  pte = walk(p->kpagetable , va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}


void
check_kvm_mapping(pagetable_t kpgtbl, uint64 va)
{
  pte_t *pte = walk(kpgtbl, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0)
    printf("warning: unmapped kernel address %p\n", va);
}


// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
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

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
/**
What uvmunmap() does
uvmunmap() walks through the virtual address range of the user process.
For each page:
- It finds the corresponding PTE (using walk()).
- Checks that it’s valid and that it’s a leaf mapping (it maps a physical page).
- If do_free == 1, it frees the physical memory (kfree((void*)pa)).
- It clears the PTE (*pte = 0):
  PTE_V is cleared (no longer valid)
  All other bits (R/W/X) are cleared too

So after uvmunmap(), 
- all user physical memory pages are freed, 
- and the leaf entries are now empty slots.
- The only remaining valid (PTE_V) entries are those that point to child page table pages, 
  i.e. the internal nodes of the page table tree.
*/

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
      // panic("uvmunmap: not mapped");
      // ============ demo code: page faults for lazy allocation ============ 
      // when using lazy allocation, the page fault handler will allocate the page and update the PTE 
       continue;
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

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

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
/*
Each page table page has 512 entries (512 × 8 bytes = 4KB).
Each entry (a PTE, page table entry) can either:
- Point to a physical page of data (a leaf entry): 
  Leaf physical pages — the actual data pages (code, heap, stack, device pages) that PTEs point to.
- Point to another page table (a non-leaf entry):
   Non-leaf page table pages — the 4KB pages that store pte entries (the tree structure).
The bit flags in the PTE indicate which case it is.

freewalk(pagetable_t pagetable) frees all page-table pages (the intermediate tables), the memory used by the page-table pages themselves. 
but not the leaf physical memory pages that the PTEs map to.
In other words, it dismantles the structure of the page table tree, not the data.


*/
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

/** xv6-labs-2020 lab3:Print a page table (easy)
 * print that pagetable in the format described below.
 */
void vmprint(pagetable_t pagetable){
  printf("page table %p\n", pagetable);
  vmprint_level(pagetable, 1);
}

void vmprint_level(pagetable_t pagetable, uint64 level){
  for(int i=0;i<512;i++){
    pte_t pte = pagetable[i];
    // only print the valid ptes
    if(pte & PTE_V){
      uint64 child = PTE2PA(pte);
      for(int j=0;j<level;j++){
        if(j>0){
          printf(" ");
        }
        printf("..");}
      printf("%d: pte %p pa %p\n", i, pte, child);
      // printf("%d: pte %p pa %p flag %p\n", i, pte, child, pte&0x3ff);

      // check if this PTE points to another page table
      if((pte & (PTE_R|PTE_W|PTE_X))==0)
        // not a leaf → recurse
        vmprint_level((pagetable_t)child, level+1);
  }
}
}


// Free user memory pages,
// then free page-table pages.
/**
A process’s address space has two distinct layers of memory management:
- User pages — the actual physical memory where user code and data live.
Each of these pages is represented by a leaf PTE (a Page Table Entry with R/W/X bits set).
- Page table pages — the internal pages that store the page table structure itself.
These are just like directories of PTEs, forming a three-level tree (in Sv39).

When a process exits, we need to clean up both:
- The content (user memory pages), and
- The structure (page table pages).
But we have to do it in that order, or we’ll lose the map before knowing what to free.

- uvmunmap() uses the page table structure to locate and free the user’s pages.
- freewalk() then dismantles the empty page table structure itself.

*/ 
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}


void
freewalk_noleaf(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // this PTE points to a lower-level page table
        uint64 child = PTE2PA(pte);
        freewalk_noleaf((pagetable_t)child);
      }
      pagetable[i] = 0;
    }else if(pte & PTE_V){
      // else, it's a leaf PTE — skip freeing its physical memory
      panic("proc free kept: leaf");
    }
  }
  kfree((void*)pagetable);
}


/* =======  end of solution for pgtbl ============ */


// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

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

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
/**
A. Old design:
Kernel page table
 ├── kernel memory mappings
 └── no user mappings (user VAs invalid here)

copyin():
  user VA → walk user page table → physical address → copy

B. New design (after per-process kernel pagetable):
Per-process kernel page table
 ├── kernel memory mappings
 └── user VA → mapped to same physical pages as user space

copyin_new():
  user VA → hardware translation → copy directly



After you implement per-process kernel page tables (in Lab 3’s earlier parts), each process now has a kernel page table p->kpagetable.
This page table includes:
- All the usual kernel mappings (KERNBASE, devices, trampoline, etc.)
- Plus mappings for that process’s user memory region (the range 0 → p->sz)

Crucially, these user mappings in the kernel page table:
- Point to the same physical pages as the user page table,
- But do not set PTE_U (so they’re accessible in kernel mode),
- And have normal read/write permissions (so kernel can safely memmove() from them).

Thus, when the kernel runs with that process’s kernel page table loaded (which it does when handling system calls, traps, etc.), 
the MMU automatically knows how to translate user virtual addresses.

So The kernel no longer needs to manually walk the user page table — the MMU already does it.
That’s why copyin_new is simply:  memmove(dst, (void*)srcva, len);
*/
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
  
  // =========== solution for pgtbl ---- part 3 =============
  return copyin_new(pagetable, dst, srcva, len);
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  // uint64 n, va0, pa0;
  // int got_null = 0;

  // while(got_null == 0 && max > 0){
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);
  //   if(pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if(n > max)
  //     n = max;

  //   char *p = (char *) (pa0 + (srcva - va0));
  //   while(n > 0){
  //     if(*p == '\0'){
  //       *dst = '\0';
  //       got_null = 1;
  //       break;
  //     } else {
  //       *dst = *p;
  //     }
  //     --n;
  //     --max;
  //     p++;
  //     dst++;
  //   }

  //   srcva = va0 + PGSIZE;
  // }
  // if(got_null){
  //   return 0;
  // } else {
  //   return -1;
  // }

  // =========== solution for pgtbl ---- part 3 =============
  return copyinstr_new(pagetable, dst, srcva, max);
}


