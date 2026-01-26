# Lab: locks
In this lab you'll gain experience in re-designing code to increase parallelism. A common symptom of poor parallelism on multi-core machines is high lock contention. Improving parallelism often involves changing both data structures and locking strategies in order to reduce contention. You'll do this for the xv6 memory allocator and block cache.

# Memory allocator (moderate)

The program `user/kalloctest` stresses xv6's memory allocator: three processes grow and shrink their address spaces, resulting in many calls to kalloc and kfree. `kalloc` and `kfree` obtain `kmem.lock`. kalloctest prints (as "#fetch-and-add") the number of loop iterations in acquire due to attempts to acquire a lock that another core already holds, for the kmem lock and a few other locks. The number of loop iterations in acquire is a rough measure of lock contention. The output of kalloctest looks similar to this before you complete the lab:


```bash
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 13449 #acquire() 433086
lock: bcache: #fetch-and-add 0 #acquire() 2106
--- top 5 contended locks:
lock: proc: #fetch-and-add 35614 #acquire() 303499
lock: proc: #fetch-and-add 28073 #acquire() 303501
lock: proc: #fetch-and-add 21103 #acquire() 303545
lock: virtio_disk: #fetch-and-add 15129 #acquire() 126
lock: kmem: #fetch-and-add 13449 #acquire() 433086
tot= 13449
test1 FAIL
start test2
total free number of pages: 32499 (out of 32768)
.....
test2 OK

```


`acquire` maintains, for each lock, the count of calls to acquire for that lock, and the number of times the loop in acquire tried but failed to set the lock. kalloctest calls a system call that causes the kernel to print those counts for the kmem and bcache locks (which are the focus of this lab) and for the 5 most contended locks. If there is lock contention the number of acquire loop iterations will be large. The system call returns the sum of the number of loop iterations for the kmem and bcache locks.

For this lab, you must use a dedicated unloaded machine with multiple cores. If you use a machine that is doing other things, the counts that kalloctest prints will be nonsense. You can use a dedicated Athena workstation, or your own laptop, but don't use a dialup machine.

The root cause of lock contention in `kalloctest` is that `kalloc()` has a single free list, protected by a single lock. To remove lock contention, you will have to redesign the memory allocator to avoid a single lock and list. 

The basic idea is to maintain a free list per CPU, each list with its own lock. Allocations and frees on different CPUs can run in parallel, because each CPU will operate on a different list. The main challenge will be to deal with the case in which one CPU's free list is empty, but another CPU's list has free memory; in that case, the one CPU must "steal" part of the other CPU's free list. Stealing may introduce lock contention, but that will hopefully be infrequent.


Your job is to implement per-CPU freelists, and stealing when a CPU's free list is empty. You must give all of your locks names that start with "kmem". That is, you should call `initlock` for each of your locks, and pass a name that starts with "kmem". 

## Test
Run kalloctest to see if your implementation has reduced lock contention. To check that it can still allocate all of memory, run `usertests sbrkmuch`. Your output will look similar to that shown below, with much-reduced contention in total on kmem locks, although the specific numbers will differ. Make sure all tests in usertests pass. make grade should say that the kalloctests pass.

```bash

$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 42843
lock: kmem: #fetch-and-add 0 #acquire() 198674
lock: kmem: #fetch-and-add 0 #acquire() 191534
lock: bcache: #fetch-and-add 0 #acquire() 1242
--- top 5 contended locks:
lock: proc: #fetch-and-add 43861 #acquire() 117281
lock: virtio_disk: #fetch-and-add 5347 #acquire() 114
lock: proc: #fetch-and-add 4856 #acquire() 117312
lock: proc: #fetch-and-add 4168 #acquire() 117316
lock: proc: #fetch-and-add 2797 #acquire() 117266
tot= 0
test1 OK
start test2
total free number of pages: 32499 (out of 32768)
.....
test2 OK
$ usertests sbrkmuch
usertests starting
test sbrkmuch: OK
ALL TESTS PASSED
$ usertests
...
ALL TESTS PASSED
$
```


## Some hints:
- You can use the constant `NCPU` from `kernel/param.h`
- Let `freerange` give all free memory to the CPU running freerange.
- The function `cpuid` returns the current core number, but it's only safe to call it and use its result when interrupts are turned off. You should use `push_off()` and `pop_off()` to turn interrupts off and on.
- Have a look at the `snprintf` function in `kernel/sprintf.c` for string formatting ideas. It is OK to just name all locks "kmem" though.

## Debug 

In xv6 terms:
- #acquire() → total times acquire(lock) was called
- #fetch-and-add → how many times a CPU had to spin because the lock was already held
- Spin count ≈ contention
Zero spin means: every acquire succeeded immediately.

### debug kalloctest
```bash
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 88864
lock: kmem: #fetch-and-add 0 #acquire() 171806
lock: kmem: #fetch-and-add 0 #acquire() 172399
lock: bcache: #fetch-and-add 0 #acquire() 340
--- top 5 contended locks:
lock: proc: #fetch-and-add 47767 #acquire() 394129
lock: proc: #fetch-and-add 19640 #acquire() 394185
lock: proc: #fetch-and-add 11451 #acquire() 394186
lock: proc: #fetch-and-add 5105 #acquire() 394135
lock: proc: #fetch-and-add 4270 #acquire() 394116
tot= 0
test1 OK
start test2
total free number of pages: 32499 (out of 32768)
.....
test2 O


```


# debug usertests sbrkmuch
```bash
usertests starting
test sbrkmuch: OK
ALL TESTS PASSED
```

# debug usertests
```bash
$ usertests
...
ALL TESTS PASSED
$
```


This half of the assignment is independent from the first half; you can work on this half (and pass the tests) whether or not you have completed the first half.

# Buffer cache (hard)


If multiple processes use the file system intensively, they will likely contend for `bcache.lock`, which protects the disk block cache in `kernel/bio.c`. 
`bcachetest` creates several processes that repeatedly read different files in order to generate contention on `bcache.lock`; its output looks like this (before you complete this lab):


```bash

$ bcachetest
start test0
test0 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 33035
lock: bcache: #fetch-and-add 16142 #acquire() 65978
--- top 5 contended locks:
lock: virtio_disk: #fetch-and-add 162870 #acquire() 1188
lock: proc: #fetch-and-add 51936 #acquire() 73732
lock: bcache: #fetch-and-add 16142 #acquire() 65978
lock: uart: #fetch-and-add 7505 #acquire() 117
lock: proc: #fetch-and-add 6937 #acquire() 73420
tot= 16142
test0: FAIL
start test1
test1 OK
```


You will likely see different output, but the number of acquire loop iterations for the `bcache` lock will be high. If you look at the code in `kernel/bio.c`, you'll see that `bcache.lock` protects the list of cached block buffers, the reference count (`b->refcnt`) in each block buffer, and the identities of the cached blocks (`b->dev` and `b->blockno`).


Modify the block cache so that the number of acquire loop iterations for all locks in the bcache is close to zero when running `bcachetest`. 
Ideally the sum of the counts for all locks involved in the block cache should be zero, but it's OK if the sum is less than 500. 

Modify `bget` and `brelse` so that concurrent lookups and releases for different blocks that are in the bcache are unlikely to conflict on locks (e.g., don't all have to wait for bcache.lock). You must maintain the invariant that at most one copy of each block is cached. 

## Test
When you are done, your output should be similar to that shown below (though not identical). Make sure `usertests` still passes. make grade should pass all tests when you are done.


```bash
$ bcachetest
start test0
test0 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 32954
lock: kmem: #fetch-and-add 0 #acquire() 75
lock: kmem: #fetch-and-add 0 #acquire() 73
lock: bcache: #fetch-and-add 0 #acquire() 85
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4159
lock: bcache.bucket: #fetch-and-add 0 #acquire() 2118
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4274
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4326
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6334
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6321
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6704
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6696
lock: bcache.bucket: #fetch-and-add 0 #acquire() 7757
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6199
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4136
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4136
lock: bcache.bucket: #fetch-and-add 0 #acquire() 2123
--- top 5 contended locks:
lock: virtio_disk: #fetch-and-add 158235 #acquire() 1193
lock: proc: #fetch-and-add 117563 #acquire() 3708493
lock: proc: #fetch-and-add 65921 #acquire() 3710254
lock: proc: #fetch-and-add 44090 #acquire() 3708607
lock: proc: #fetch-and-add 43252 #acquire() 3708521
tot= 128
test0: OK
start test1
test1 OK
$ usertests
  ...
ALL TESTS PASSED
$
```

Please give all of your locks names that start with "bcache". That is, you should call `initlock` for each of your locks, and pass a name that starts with "bcache".

Reducing contention in the block cache is more tricky than for `kalloc`, because `bcache` buffers are truly shared among processes (and thus CPUs). 
For `kalloc`, one could eliminate most contention by giving each CPU its own allocator; that won't work for the block cache. 
We suggest you look up block numbers in the cache with a **hash table** that has a lock per hash bucket.

There are some circumstances in which it's OK if your solution has lock conflicts:
- When two processes concurrently use the same block number. bcachetest test0 doesn't ever do this.
- When two processes concurrently miss in the cache, and need to find an unused block to replace. bcachetest test0 doesn't ever do this.
- When two processes concurrently use blocks that conflict in whatever scheme you use to partition the blocks and locks; for example, if two processes use blocks whose block numbers hash to the same slot in a hash table. bcachetest test0 might do this, depending on your design, but you should try to adjust your scheme's details to avoid conflicts (e.g., change the size of your hash table).
bcachetest's test1 uses more distinct blocks than there are buffers, and exercises lots of file system code paths.

## hints
Here are some hints:

- Read the description of the block cache in the xv6 book (Section 8.1-8.3).
- It is OK to use a fixed number of buckets and not resize the hash table dynamically. Use a prime number of buckets (e.g., 13) to reduce the likelihood of hashing conflicts.
- Searching in the hash table for a buffer and allocating an entry for that buffer when the buffer is not found must be atomic.
- Remove the list of all buffers (bcache.head etc.) and instead time-stamp buffers using the time of their last use (i.e., using ticks in kernel/trap.c). With this change `brelse` doesn't need to acquire the bcache lock, and `bget` can select the least-recently used block based on the time-stamps.
- It is OK to serialize `eviction` in `bget` (i.e., the part of bget that selects a buffer to re-use when a lookup misses in the cache).
- Your solution might need to hold two locks in some cases; for example, during eviction you may need to hold the bcache lock and a lock per bucket. Make sure you avoid deadlock.

- When replacing a block, you might move a struct buf from one bucket to another bucket, because the new block hashes to a different bucket. You might have a tricky case: the new block might hash to the same bucket as the old block. Make sure you avoid deadlock in that case.

- Some debugging tips: implement bucket locks but leave the global bcache.lock acquire/release at the beginning/end of bget to serialize the code. Once you are sure it is correct without race conditions, remove the global locks and deal with concurrency issues. You can also run make CPUS=1 qemu to test with one core.

## Solution

### Big Picture

We replace the single global LRU list + single bcache.lock with:
- A hash table of buckets
- One lock per bucket
- A serialized eviction path
- A global invariant: one (dev, blockno) → one buffer



The original design flaw (intentional), xv6 starts simple:
- One global `bcache.lock`
- Every lookup, refcnt change, and release goes through it
- Correct, but terrible under contention


This lab is not about hash tables. It’s about lock granularity and invariants.
The invariant you must preserve: At most one cached buffer exists for (dev, blockno)

Why this passes bcachetest: 
- Different blocks → different buckets → no lock conflict
- Same block → correct serialization
- Eviction rare and serialized → acceptable
- Invariant preserved: one cached copy per block


### Step 1: Split the lock’s responsibilities

The original bcache.lock is doing too much.
You split it into:
- Bucket locks → protect lookup and insertion
- Optional global lock → protect eviction
Each lock has a narrow job. That’s the whole trick.

### Step 2: Hash table of buckets
You introduce: Fixed number of buckets (prime number, e.g. 13 or 17)

Each bucket: 
- Has a spinlock
- Contains a list of buffers whose (dev, blockno) hash there

Now In the hash-table-based bcache design::
- Different blocks → likely different locks
- Parallelism increases immediately
- Each bucket corresponds to a range of block numbers (via a hash function).
- A bucket contains buffers currently caching blocks whose (dev, blockno) hash to that bucket.
  
This alone kills most contention in test0.

So the invariant is:
A struct buf lives in the bucket determined by `hash(b->dev, b->blockno)`

### Step 3: Buffer ownership model (critical)

Each struct buf has two phases of ownership:
1. Metadata ownership :Controlled by bucket lock
- dev, blockno
- refcnt
- hash linkage

2. Data ownership : Controlled by buf.lock (sleep lock)
- data[]
- disk I/O
- read/write safety

Never confuse these.

### Step 4: How bget works (high level)
Think in phases, not lines of code.
1. Phase A: Lookup
- Compute hash
- Acquire bucket lock
- Scan bucket list
  If found:
    - Increment refcnt
    - Release bucket lock
    - Acquire buf.lock
    - Return buffer

No global lock. No disk. Fast.

2. Phase B: Miss
- You didn’t find the block
- Now you must create exactly one buffer for it

This is the only hard part.


### Step 5: Serialized eviction (allowed and encouraged)

Eviction is rare and expensive. So we intentionally serialize it.
You may:
- Acquire **a global eviction lock**
- Scan buffers to find a victim (refcnt == 0)
- Remove it from its old bucket
- Reassign (dev, blockno)
- Insert into new bucket
- Set refcnt = 1

This lock can be coarse. That’s fine.
bcachetest test0 doesn’t stress eviction.

Correctness > elegance here.

Eviction is not “freeing” a buffer.
Eviction means:
- Take a struct buf that currently represents block A
- Reassign it to represent block B
  
```c
b->dev = new_dev;
b->blockno = new_blockno;
b->valid = 0;

```

#### Why this specific part must be serialized

Suppose:
- Old block number: blockno = 100
- New block number: blockno = 9001
- Hash function: blockno % NBuckets

Example:
- 100 % 13 = 9
- 9001 % 13 = 5

So before eviction: Buffer b must be in bucket 9
After eviction:     Buffer b must be in bucket 5
Same buffer. Different bucket.
That’s not optional — it’s required to maintain lookup correctness.



Eviction is dangerous because it involves:
- 1. Choosing a victim (refcnt == 0)
- 2. Changing its identity (dev, blockno)
- 3. Moving it between buckets (Each bucket has its own lock.)
(Once you shard the cache by hash buckets, identity movement becomes a real operation.)

Those steps must be atomic with respect to other evictions.

Bucket locks do not prevent this, because:
- The victim buffer may be in a different bucket
- Or not in any bucket yet
- Or transitioning between buckets

So eviction may require:
- Lock A (old bucket)
- Lock B (new bucket)

And: A global eviction lock to stop other evictions from racing you

So you need a single choke point.

So you introduce:  One global spinlock (e.g. `bcache.evict_lock`)
Held only while:
- Scanning for a free buffer
- Reassigning it
- Re-inserting into a bucket

This lock is:
- Rarely contended
- Never held during disk I/O
- Never held during lookup hits

Performance stays high.

### Step 6: brelse becomes almost trivial
This is a quiet win.
- Acquire bucket lock
- Decrement refcnt
- Update LRU timestamp (optional)
- Release bucket lock
- Release buf.lock
No global lock needed. No list shuffling. No contention storm.


## Deadlock rules you must obey
Write these on a sticky note:
- Never sleep while holding a spinlock
- Bucket lock before buf.lock
- If holding two spinlocks, acquire in a consistent order
- Eviction lock > bucket lock (if both needed)

If you violate any of these, xv6 will punish you creatively.

## Why this design passes bcachetest

- test0: different blocks → different buckets → no contention
- test1: eviction serialized → correctness preserved
- Disk I/O still uses sleep locks → no CPU burning
- fetch-and-add loops drop to near zero → success

This is textbook scalable kernel design, not a hack.


## Debug

1. Implement only bucketed lookup, keep eviction serialized
2. Test with CPUS=1
3. Then test CPUS=4
4. Only then remove any leftover global locks

```bash
$ bcachetest
start test0
test0 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 399524
lock: kmem: #fetch-and-add 0 #acquire() 824424
lock: kmem: #fetch-and-add 0 #acquire() 920987
lock: kmem: #fetch-and-add 0 #acquire() 39
lock: kmem: #fetch-and-add 0 #acquire() 39
lock: kmem: #fetch-and-add 0 #acquire() 39
lock: kmem: #fetch-and-add 0 #acquire() 39
lock: kmem: #fetch-and-add 0 #acquire() 39
lock: bcache.eviction: #fetch-and-add 13446 #acquire() 25020
lock: bcache.bucket: #fetch-and-add 5 #acquire() 167373
lock: bcache.bucket: #fetch-and-add 0 #acquire() 161307
lock: bcache.bucket: #fetch-and-add 0 #acquire() 107236
lock: bcache.bucket: #fetch-and-add 0 #acquire() 82442
lock: bcache.bucket: #fetch-and-add 0 #acquire() 54784
lock: bcache.bucket: #fetch-and-add 0 #acquire() 29113
lock: bcache.bucket: #fetch-and-add 0 #acquire() 27396
lock: bcache.bucket: #fetch-and-add 0 #acquire() 27238
lock: bcache.bucket: #fetch-and-add 0 #acquire() 27209
lock: bcache.bucket: #fetch-and-add 0 #acquire() 28150
lock: bcache.bucket: #fetch-and-add 181 #acquire() 68885
lock: bcache.bucket: #fetch-and-add 28 #acquire() 343277
lock: bcache.bucket: #fetch-and-add 124 #acquire() 783465
--- top 5 contended locks:
lock: proc: #fetch-and-add 71584180 #acquire() 6579148
lock: proc: #fetch-and-add 69891217 #acquire() 6583358
lock: proc: #fetch-and-add 69399118 #acquire() 6580235
lock: proc: #fetch-and-add 58454664 #acquire() 6575170
lock: log: #fetch-and-add 44630793 #acquire() 73547
tot= 13784
test0: OK
start test1
test1 OK


$ usertests
usertests starting
test manywrites: OK
test execout: OK
test copyin: OK
test copyout: OK
test copyinstr1: OK
test copyinstr2: OK
test copyinstr3: OK
test rwsbrk: OK
test truncate1: OK
test truncate2: OK
test truncate3: OK
test reparent2: OK
test pgbug: OK
test sbrkbugs: usertrap(): unexpected scause 0x000000000000000c pid=3240
            sepc=0x0000000000007106 stval=0x0000000000007106
usertrap(): unexpected scause 0x000000000000000c pid=3241
            sepc=0x0000000000007106 stval=0x0000000000007106
OK
test badarg: OK
test reparent: OK
test twochildren: OK
test forkfork: OK
test forkforkfork: OK
test argptest: OK
test createdelete: OK
test linkunlink: OK
test linktest: OK
test unlinkread: OK
test concreate: OK
test subdir: OK
test fourfiles: OK
test sharedfd: OK
test dirtest: OK
test exectest: OK
test bigargtest: OK
test bigwrite: OK
test bsstest: OK
test sbrkbasic: OK
test sbrkmuch: OK
test kernmem: usertrap(): unexpected scause 0x000000000000000d pid=6221
            sepc=0x00000000000057e2 stval=0x0000000080000000
usertrap(): unexpected scause 0x000000000000000d pid=6222
            sepc=0x00000000000057e2 stval=0x000000008000c350
usertrap(): unexpected scause 0x000000000000000d pid=6223
            sepc=0x00000000000057e2 stval=0x00000000800186a0
usertrap(): unexpected scause 0x000000000000000d pid=6224
            sepc=0x00000000000057e2 stval=0x00000000800249f0
usertrap(): unexpected scause 0x000000000000000d pid=6225
            sepc=0x00000000000057e2 stval=0x0000000080030d40
usertrap(): unexpected scause 0x000000000000000d pid=6226
            sepc=0x00000000000057e2 stval=0x000000008003d090
usertrap(): unexpected scause 0x000000000000000d pid=6227
            sepc=0x00000000000057e2 stval=0x00000000800493e0
usertrap(): unexpected scause 0x000000000000000d pid=6228
            sepc=0x00000000000057e2 stval=0x0000000080055730
usertrap(): unexpected scause 0x000000000000000d pid=6229
            sepc=0x00000000000057e2 stval=0x0000000080061a80
usertrap(): unexpected scause 0x000000000000000d pid=6230
            sepc=0x00000000000057e2 stval=0x000000008006ddd0
usertrap(): unexpected scause 0x000000000000000d pid=6231
            sepc=0x00000000000057e2 stval=0x000000008007a120
usertrap(): unexpected scause 0x000000000000000d pid=6232
            sepc=0x00000000000057e2 stval=0x0000000080086470
usertrap(): unexpected scause 0x000000000000000d pid=6233
            sepc=0x00000000000057e2 stval=0x00000000800927c0
usertrap(): unexpected scause 0x000000000000000d pid=6234
            sepc=0x00000000000057e2 stval=0x000000008009eb10
usertrap(): unexpected scause 0x000000000000000d pid=6235
            sepc=0x00000000000057e2 stval=0x00000000800aae60
usertrap(): unexpected scause 0x000000000000000d pid=6236
            sepc=0x00000000000057e2 stval=0x00000000800b71b0
usertrap(): unexpected scause 0x000000000000000d pid=6237
            sepc=0x00000000000057e2 stval=0x00000000800c3500
usertrap(): unexpected scause 0x000000000000000d pid=6238
            sepc=0x00000000000057e2 stval=0x00000000800cf850
usertrap(): unexpected scause 0x000000000000000d pid=6239
            sepc=0x00000000000057e2 stval=0x00000000800dbba0
usertrap(): unexpected scause 0x000000000000000d pid=6240
            sepc=0x00000000000057e2 stval=0x00000000800e7ef0
usertrap(): unexpected scause 0x000000000000000d pid=6241
            sepc=0x00000000000057e2 stval=0x00000000800f4240
usertrap(): unexpected scause 0x000000000000000d pid=6242
            sepc=0x00000000000057e2 stval=0x0000000080100590
usertrap(): unexpected scause 0x000000000000000d pid=6243
            sepc=0x00000000000057e2 stval=0x000000008010c8e0
usertrap(): unexpected scause 0x000000000000000d pid=6244
            sepc=0x00000000000057e2 stval=0x0000000080118c30
usertrap(): unexpected scause 0x000000000000000d pid=6245
            sepc=0x00000000000057e2 stval=0x0000000080124f80
usertrap(): unexpected scause 0x000000000000000d pid=6246
            sepc=0x00000000000057e2 stval=0x00000000801312d0
usertrap(): unexpected scause 0x000000000000000d pid=6247
            sepc=0x00000000000057e2 stval=0x000000008013d620
usertrap(): unexpected scause 0x000000000000000d pid=6248
            sepc=0x00000000000057e2 stval=0x0000000080149970
usertrap(): unexpected scause 0x000000000000000d pid=6249
            sepc=0x00000000000057e2 stval=0x0000000080155cc0
usertrap(): unexpected scause 0x000000000000000d pid=6250
            sepc=0x00000000000057e2 stval=0x0000000080162010
usertrap(): unexpected scause 0x000000000000000d pid=6251
            sepc=0x00000000000057e2 stval=0x000000008016e360
usertrap(): unexpected scause 0x000000000000000d pid=6252
            sepc=0x00000000000057e2 stval=0x000000008017a6b0
usertrap(): unexpected scause 0x000000000000000d pid=6253
            sepc=0x00000000000057e2 stval=0x0000000080186a00
usertrap(): unexpected scause 0x000000000000000d pid=6254
            sepc=0x00000000000057e2 stval=0x0000000080192d50
usertrap(): unexpected scause 0x000000000000000d pid=6255
            sepc=0x00000000000057e2 stval=0x000000008019f0a0
usertrap(): unexpected scause 0x000000000000000d pid=6256
            sepc=0x00000000000057e2 stval=0x00000000801ab3f0
usertrap(): unexpected scause 0x000000000000000d pid=6257
            sepc=0x00000000000057e2 stval=0x00000000801b7740
usertrap(): unexpected scause 0x000000000000000d pid=6258
            sepc=0x00000000000057e2 stval=0x00000000801c3a90
usertrap(): unexpected scause 0x000000000000000d pid=6259
            sepc=0x00000000000057e2 stval=0x00000000801cfde0
usertrap(): unexpected scause 0x000000000000000d pid=6260
            sepc=0x00000000000057e2 stval=0x00000000801dc130
OK
test sbrkfail: usertrap(): unexpected scause 0x000000000000000d pid=6272
            sepc=0x0000000000005a5a stval=0x0000000000014000
OK
test sbrkarg: OK
test validatetest: OK
test stacktest: usertrap(): unexpected scause 0x000000000000000d pid=6276
            sepc=0x00000000000061a4 stval=0x0000000000011b40
OK
test opentest: OK
test writetest: OK
test writebig: OK
test createtest: OK
test openiput: OK
test exitiput: OK
test iput: OK
test mem: OK
test pipe1: OK
test preempt: kill... wait... OK
test exitwait: OK
test rmdot: OK
test fourteen: OK
test bigfile: OK
test dirfile: OK
test iref: OK
test forktest: OK
test bigdir: OK
ALL TESTS PASSED

$
```
