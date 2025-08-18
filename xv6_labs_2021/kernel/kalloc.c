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

/**
 * A run is a free memory block (one page = 4096 bytes).
 * The struct run itself is stored at the start of that free page.
 * next points to the next free page in the allocator’s free list.
So the free memory is maintained as a linked list of free pages.
So freelist points to the first run, and you can traverse through all free pages.
 */
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

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
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
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
