// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

/** What is kmem? 
 * kmem is a global variable (static struct).
 * - Memory allocation is a system-wide resource — all kernel subsystems need to allocate/free pages.
 * - Having a single global kmem.freelist ensures there is one central pool of available physical pages.
 * - Protected by a spinlock, so multiple CPUs can safely allocate/free concurrently.
 * 
 * struct run is the node type — each free physical page is represented by a struct run stored inside it.
 * A run is a free memory block (one page = 2^12 bytes=  4096 bytes = 4 KB).
 * The struct run itself is stored at the start of that free page.
 * next points to the next free page in the allocator’s free list.
So the free memory is maintained as a linked list of free pages.
So freelist points to the first run, and you can traverse through all free pages.

 What is kmem.freelist?
 - kmem.freelist manages physical 4 KB pages.
 - kmem.freelist is a linked list of all free physical memory pages.
 - Each free page is represented by a struct run, and the next field points to the next free page.
 So kmem.freelist is just the head pointer of the linked list of available pages.
 So when a physical page is free, xv6 reuses the first few bytes of that page to store the struct run.
That way the kernel doesn’t need extra memory to track free pages.

When a page is allocated via kalloc(), the function removes it from the front of this linked list, 
when a page is freed via kfree(), it's added back to the front of this same list.

 */
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


/**
When the kernel boots:
- freerange() walks through physical memory and adds each page to kmem.freelist.
- Later, kalloc() removes one page from the front of the list.
- kfree() adds a page back to the front of the list.

 */
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
/**

 r!=0: 
 In C, r = 0 (or equivalently r == NULL) means:  there are no free pages left in the free list.
 If kmem.freelist is non-null, it points to a free page (struct run *), and r is set to that page.
 *If kmem.freelist is null (0), it means the kernel’s free memory pool is empty, so r = 0.

 If r is non-null, kalloc() returns a pointer to the allocated page.
 If r == 0, (void*)r=(void*)0,  That means kalloc() will return the null pointer, which signals out of memory.

 When you allocate memory with kalloc(), what you’re really asking is:
 “Please give me one 4 KB physical page that I can later map into a process’s virtual address space.”
 */
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


/**
 * Function To collect the amount of free memory: 
 * We walk the free list and count how many pages are free.
 * 
 * xv6 allocates memory in pages (4KB each).
 * The free memory is tracked by a free list (freelist) in kalloc.c.
 * Each free block is a struct run linked list.
 */
uint64
freemomory(void)
{ 
  struct run *r;
  uint64 freebytes = 0;
  acquire(&kmem.lock);  // Protect with the spinlock kmem.lock because freelist can change in parallel.
  for(r = kmem.freelist; r; r = r->next)
    freebytes += PGSIZE;
  release(&kmem.lock);
  return freebytes;

}
