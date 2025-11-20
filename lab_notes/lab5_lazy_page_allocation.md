Lab: xv6 lazy page allocation

One of the many neat tricks an O/S can play with page table hardware is lazy allocation of user-space heap memory. Xv6 applications ask the kernel for heap memory using the `sbrk()` system call. In the kernel we've given you, sbrk() allocates physical memory and maps it into the process's virtual address space. It can take a long time for a kernel to allocate and map memory for a large request. Consider, for example, that a gigabyte consists of 262,144 4096-byte pages; that's a huge number of allocations even if each is individually cheap. In addition, some programs allocate more memory than they actually use (e.g., to implement sparse arrays), or allocate memory well in advance of use. 

To allow sbrk() to complete more quickly in these cases, sophisticated kernels allocate user memory lazily. That is, sbrk() doesn't allocate physical memory, but just remembers which user addresses are allocated and marks those addresses as invalid in the user page table. When the process first tries to use any given page of lazily-allocated memory, the CPU generates a page fault, which the kernel handles by allocating physical memory, zeroing it, and mapping it. You'll add this lazy allocation feature to xv6 in this lab.


# Eliminate allocation from sbrk() (easy)

v
Your new sbrk(n) should just increment the process's size (myproc()->sz) by n and return the old size. It should not allocate memory -- so you should delete the call to `growproc()` (but you still need to increase the process's size!).
Try to guess what the result of this modification will be: what will break?

Make this modification, boot xv6, and type echo hi to the shell. You should see something like this:

```bash
init: starting sh
$ echo hi
usertrap(): unexpected scause 0x000000000000000f pid=3
            sepc=0x0000000000001258 stval=0x0000000000004008
va=0x0000000000004000 pte=0x0000000000000000
panic: uvmunmap: not mapped

```

The "usertrap(): ..." message is from the user trap handler in trap.c; it has caught an exception that it does not know how to handle. Make sure you understand why this page fault occurs. The "stval=0x0..04008" indicates that the virtual address that caused the page fault is 0x4008.



# Lazy allocation (moderate)
Modify the code in `trap.c` to respond to a page fault from user space by mapping a newly-allocated page of physical memory at the faulting address, and then returning back to user space to let the process continue executing. You should add your code just before the printf call that produced the "usertrap(): ..." message. Modify whatever other xv6 kernel code you need to in order to get echo hi to work.

## Here are some hints:
- You can check whether a fault is a page fault by seeing if `r_scause()` is 13 or 15 in usertrap().
- `r_stval()` returns the RISC-V stval register, which contains the virtual address that caused the page fault.
- Steal code from `uvmalloc()` in `vm.c`, which is what `sbrk()` calls (via `growproc()`). You'll need to call `kalloc()` and `mappages()`.
- Use `PGROUNDDOWN(va)` to round the faulting virtual address down to a page boundary.
- `uvmunmap()` will panic; modify it to not panic if some pages aren't mapped.
- If the kernel crashes, look up `sepc` in kernel/kernel.asm
- Use your `vmprint` function from pgtbl lab to print the content of a page table.
  
If you see the error "incomplete type proc", include "spinlock.h" then "proc.h".
If all goes well, your lazy allocation code should result in echo hi working. You should get at least one page fault (and thus lazy allocation), and perhaps two.



# Lazytests and Usertests (moderate)

We've supplied you with lazytests, an xv6 user program that tests some specific situations that may stress your lazy memory allocator. Modify your kernel code so that all of both lazytests and usertests pass.

- Handle negative `sbrk()` arguments.
- Kill a process if it page-faults on a virtual memory address higher than any allocated with sbrk().
- Handle the parent-to-child memory copy in `fork()` correctly.
- Handle the case in which a process passes a valid address from `sbrk()` to a system call such as read or write, but the memory for that address has not yet been allocated.
- Handle out-of-memory correctly: if `kalloc()` fails in the page fault handler, kill the current process.
- Handle faults on the invalid page below the user stack.

## Test

Your solution is acceptable if your kernel passes lazytests and usertests:
```bash
$ lazytests
lazytests starting
running test lazy alloc
test lazy alloc: OK
running test lazy unmap...
usertrap(): ...
test lazy unmap: OK
running test out of memory
usertrap(): ...
test out of memory: OK
ALL TESTS PASSED
$ usertests
...
ALL TESTS PASSED
$
```

## Solution

### workflow
1. 修改 sys_sbrk：扩张仅改 p->sz；缩小时循环从 PGROUNDUP(newsz) 到 PGROUNDUP(oldsz)，若 pte 有 PTE_V 释放物理页并取消映射。
2. 修改 usertrap：当 scause 指示页故障，检查 stval 所在页是否 < p->sz，若是，kalloc、memset、mappages；若超出 p->sz，kill。
3. 修改 uvmunmap：遇到未映射页不要 panic；只有 PTE_V 时才 kfree。
4. 修改 uvmcopy（fork 复制）：只有父进程中 已映射 的页才为子分配并拷贝；父未映射的页在子中也保持未映射（lazy）。
5. 修改 copyin/copyout：当 kernel 尝试读写用户缓冲未映射页且 va < p->sz 时，在内核中分配并映射（与 usertrap 重用逻辑）。
6. 在 page-fault 分配失败时 kill 进程（OOM）。
7. 对于栈下的保护页/guard 页，根据内核栈布局，遇到该页故障时 kill。
   


### 处理 sbrk 的负数参数

sbrk(n) 要改变用户进程的地址空间大小（p->sz）。

- 正数 sbrk = 修改 sz，但不分配
  正数很好办：p->sz += n，等到访问时 page fault 再 lazy 分配。
- 负数 sbrk = 修改 sz，但要回收 real pages（如果有）并 unmap


可 负数 要缩小地址空间，这意味着：
1. p->sz 要减小。
2. 原来高地址的那部分已经 lazily 分配的物理页必须被 unmap & free。
   - unmap 删页表, 不会释放物理内存。只负责撤销虚拟地址 → 物理地址 的映射关系, 
   - free: 还物理内存，不动页表。 必须拿到 物理页的地址（通常来自 PTE2PA）,然后把这整页物理内存放回内核的物理内存分配器（kmem.freelist）,free 不会动页表
  
3. 没有被分配（lazy still not touched）的那些虚拟地址，只需要删掉页表映射 unmap
   因为 lazy allocation 期间，有些虚拟地址从没触发 page fault，因此它们没被真正分配过：
   - 没有页表项
   - 没有物理内存
   这类地址 shrink 时： 不能 free, 必须 unmap（确保页表里不存在残留）

   

本质上，你要写的是一个 lazy 版本的 shrink

关键点：用 oldsz → newsz 去删页
xv6 之前的 growproc() 会用 uvmdealloc() 去回收内存，但现在你不能简单调用它，因为：
- uvmdealloc 假设页都是真实分配过的；
- 你的 lazy 实现里，很多页表项根本不存在。

循环从 PGROUNDUP(newsz) 到 PGROUNDUP(oldsz) 之间每页：
删除区间 = [ PGROUNDUP(newsz), PGROUNDUP(oldsz) )

- 如果 pte 存在并且它是有效页（PTE_V），那就 kfree();
- 无论如何都要把页表映射删掉（清掉 PTE）。

进程从 oldsz 缩小到 newsz：
- 小于 newsz 的那些字节必须保留
- 大于等于 newsz 的所有字节都应该被丢弃
但实际释放需要按页为单位
页的判断方式是：
- 如果一页的起始地址 >= newsz 所在页之后的下一整页，这整页就应该被删。
- 反之，如果一页部分仍落在保留区（newsz 所在页），这页不能删。


例子：oldsz = 6000, newsz = 3000
页大小 4096。
```

0 ────────────────────────────────
  |         [page 0: 0~4095]       |
  |         [page 1: 4096~8191]    |
8192 ─────────────────────────────


```
### bounds-check


```c
if (va >= p->sz || va >= MAXVA) { 
  p->killed = 1; 
  goto out_usertrap; 
  }
```

Here’s a clean, readable **logical memory diagram** of xv6 user space, drawn **from high addresses (top) to low addresses (bottom)**. After that, I’ll re-explain the bounds-check using the picture.

I’ll keep it tight and visual—you’ll see why the check is needed.

---

#### **xv6 User Virtual Memory Layout (HIGH → LOW)**

```
   +-------------------------------------------------------------+  MAXVA (~ 2^38)
   |                invalid / kernel / guard area                |
   +-------------------------------------------------------------+
   |                                                             |
   |  TRAMP: trampoline code (at the very top of user VA space)  |
   +-------------------------------------------------------------+
   |                 user stack (grows downward)                 |
   |                       (allocated by exec)                   |
   +-------------------------------------------------------------+
   |                                                             |
   |                 ~~~~~~~~~  HOLE  ~~~~~~~~~                  |
   |       (unallocated memory user code must not touch)         |
   |                                                             |
   +-------------------------------------------------------------+
   |                                                             |
   |                     heap (grows upward)                     |
   |                     allocated by sbrk()                     |
   |                                                             |
   +-------------------------- p->sz ----------------------------+  ← p->sz
   |                         bss segment                         |
   +-------------------------------------------------------------+
   |                         data segment                        |
   +-------------------------------------------------------------+
   |                         text segment                        |
   +-------------------------------------------------------------+
   |                       low addresses                         |
   +-------------------------------------------------------------+   0
```

The heap grows upward via `sbrk()` and its current size is stored in: `p->sz`
Meaning: every user virtual address in `[0, p->sz)` is valid user space. Anything ≥ p->sz is not allocated.

You can think of `p->sz` as “the top of the currently allocated heap.”
Everything **below** `p->sz` is potentially mappable by lazy allocation.
Everything **above** `p->sz` is forbidden except the stack region that exec sets up.

---


Your lazy page fault handler contains:

```c
if (va >= p->sz || va >= MAXVA) {
    p->killed = 1;
    goto out_usertrap;
}
```

This is protecting two separate rules.

---

#### **Rule 1 — `va >= p->sz`**

Inside your diagram:

```
   HIGH ADDR
      ...
      HOLE   ← touching here is ILLEGAL
      ...
   p->sz  ← top of actual allocated memory
      ...
   LOW ADDR
```

When a page fault occurs, lazy allocation says:

> “I will allocate a page **only if** the faulting address lies inside the heap region the process owns.”

The heap region is `[0, p->sz)`.

So if:

```
va >= p->sz
```

that means the address lies in the **HOLE** region (or beyond), i.e. memory the process never asked for.

Examples:

* Program does `*(char*)0x5000 = 1`, but `p->sz = 0x4000`
* Program jumps wildly to some unmapped code region
* Buggy pointer arithmetic

Lazy allocation *must not* silently allow this.
So we kill the process.

---

#### **Rule 2 — `va >= MAXVA`**

Look at the top of the address space:

```
   +-------------------------------------------------+ MAXVA
   | kernel, guard, illegal                          |
   +-------------------------------------------------+
   | trampoline (at the very top of user address)    |
   +-------------------------------------------------+
```

Anything at or above MAXVA is never valid for user.
Not heap, not stack, not code—nothing.

This prevents user processes from touching:

* trampoline page
* direct-mapped kernel addresses
* guard pages

So if:

```
va >= MAXVA
```

→ kill the process.

---

**Putting it together with the diagram**

```
   HIGH ADDR
   +-----------------------------------------------------+ MAXVA
   |  TRAMPOLINE / INVALID / KERNEL (NEVER TOUCH)        |
   +-----------------------------------------------------+
            ^  if va >= MAXVA → kill
   |------------------- stack ---------------------------|
   |------------------- HOLE ----------------------------|
            ^  if va >= p->sz → kill
   |------------------- heap (lazy alloc allowed) -------|
            ^  p->sz
   |------------------- bss/data/text -------------------|
   LOW ADDR
```

Lazy allocation is allowed **only within the heap**, not in stack or the big hole between heap and stack.

---

**One-sentence intuition**

The bounds-check enforces:
**“Lazy allocation is allowed only inside the region the process explicitly grew with `sbrk()`. Anything higher is forbidden.”**

---

If you want, I can next show you how exec sets up the stack so that page faults in the stack region should *never* be lazy-allocated — they are already allocated up front.




### 修改 uvmcopy（fork 复制）： Handle the parent-to-child memory copy in fork() correctly： 
只有父进程中 已映射 的页才为子分配并拷贝；父未映射的页在子中也保持未映射（lazy）

When fork() creates the child, copy only the already mapped pages from the parent. For each mapped page, allocate a new physical page for the child, copy the contents, and map it in the child. For parent pages that are not mapped (lazy pages), do not allocate or map anything in the child — leave them lazy there as well.

修改要点：

在 uvmcopy() 或 copyuvm() 的循环中，检查父页表 pte：

- 如果父 pte 存在且 PTE_V（已映射）：为子调用 kalloc, copy 数据, mappages 对应页。
- 如果父 pte 不存在或 !PTE_V：不要分配页，也不要建立映射；继续下一页。



### 修改 copyin/copyout：

```bash
$ usertests
usertests starting
usertrap: page fault with scause 15 kalloc failed , pid=76
test execout: usertrap: page fault with scause 15 kalloc failed , pid=78
usertrap: page fault with scause 15 kalloc failed , pid=79
usertrap: page fault with scause 15 kalloc failed , pid=80
usertrap: page fault with scause 15 kalloc failed , pid=81

test pgbug: OK
test sbrkbugs: usertrap(): unexpected scause 0x000000000000000c pid=3307
            sepc=0x0000000000006f0e stval=0x0000000000006f0e
usertrap(): unexpected scause 0x000000000000000c pid=3308
            sepc=0x0000000000006f0e stval=0x0000000000006f0e
OK
test badarg: OK

test bsstest: OK
test sbrkbasic: usertrap: page fault with scause 15 kalloc failed , pid=6283
OK
test sbrkmuch: OK
test kernmem: OK
test sbrkfail: usertrap: page fault with scause 13 kalloc failed , pid=6338
OK
test sbrkarg: sbrkarg: write sbrk failed
FAILED
test validatetest: OK
test stacktest: panic: remap

can you help to analyze the log on terminal and help to fix
```

The kernel must safely copy user data even if the pages are not yet allocated — i.e. the kernel's `copyin/copyout` must allocate pages on behalf of the user buffer or the write should return an error/kill the process appropriately.

`sbrkarg` failed with message write sbrk failed — typically this test calls `write(fd, buf, n)` where buf points into newly-sbrk’d memory that is not yet backed by physical pages (lazy). 

`copyin/copyout` do not allocate pages when the kernel tries to access a user buffer that is within `p->sz` but the page hasn't been lazily allocated yet. 
Result: syscall fails (or you hit kalloc in unexpected places), causing sbrkarg to fail.


Modify copyin() and copyout() in vm.c   so that when they encounter an unmapped user page but the VA is < p->sz, they allocate + map a page (same logic as your page-fault handler). 
If kalloc() fails, they should return an error (or set p->killed).



sbrkarg 做了：

1. 调用 sbrk(n) 扩大进程大小（增加 p->sz），但不触发任何访问

2. 然后在新扩大的一块 user 空间里写东西，比如：
char *p = sbrk(8192);
write(fd, p, 8192);


3. 关键：p 指向的是 未分配、未映射的用户页
write() → 内核 → copyin()，内核开始读 p 指向的 user address
发现没有映射 → 应该分配页面

#### 处理逻辑
1. sbrk(8192) → 增加 p->sz（但不创建页）
2. write(fd, new_area, 8192)
copyin 开始访问新区域 → walkaddr 找不到 → 返回 -1 → 内核重试
3. 用户继续访问该地址 → 触发 page fault
4. usertrap 捕获 fault
5. 检查：新地址 < p->sz → 合法 → 分配页 → 映射
6. copyin 再次执行 → 成功读出 8192 字节

这就是 lazy allocation 应有的样子。

---

#### 整体作用：从用户虚拟地址 → 内核 buffer

用户进程看到的地址（如 `srcva`）只是虚拟地址，内核要读需要：

1. 找到这个虚拟地址所在的 page（PGROUNDDOWN）。
2. 通过页表 walk 找到物理地址。
3. 拷贝一段字节过去。
4. 如果数据跨越页边界，就继续走下一页。

整个函数就是这个循环。

---

我用一句更口语化的总结：**copyin() = “按页”从 user space 读取数据到 kernel，确保每个相关的页都合法映射。**

---

#### Step 1: 定位虚拟地址所在的 page

```c
va0 = PGROUNDDOWN(srcva);
```

假设：

* srcva = 0x1234
* 页面大小 = 4096 (0x1000)

PGROUNDDOWN 会把地址压到 page 边界：

* va0 = 0x1000

这就像**先找到信封（page）的位置，再在信封里找信件（偏移）**。

---

#### Step 2: 找到页面的物理地址

```c
pa0 = walkaddr(pagetable, va0);
if(pa0 == 0)
  return -1;
```

`walkaddr` 会：

* 根据 pagetable 查页表项。
* 页存在并可访问 → 返回物理地址。
* 否则 → 返回 0，说明不能访问（例如用户传一个坏指针）。

---

#### Step 3: 计算当前页还能读多少

```c
n = PGSIZE - (srcva - va0);
if(n > len)
  n = len;
```

你可以把它理解成：

**这一页从 offset 处最多还能拿多少字节？**

举例：

* srcva = 0x1234 → offset = 0x34
* 当前页剩余字节 = 4096 - 0x34 = 4044 字节
* 但如果用户只要求拷贝 20 字节，那只读 20 就行。

---

#### Step 4: 执行实际的拷贝

```c
memmove(dst, (void *)(pa0 + (srcva - va0)), n);
```

把数据从：

```
物理地址 pa0 + page 内偏移
```

拷到：

```
内核 dst
```

这一步很关键：**memmove 用的是物理地址方向的指针，不是虚拟地址**，因为内核处理的是物理内存。

---

#### Step 5: 更新指针，进入下一页

```c
len -= n;
dst += n;
srcva = va0 + PGSIZE;
```

意思是：

* 已经完成 n 字节，继续读剩余的
* kernel buffer 指针往后推
* 用户虚拟地址跳到下一页开始

---

#### 关键点总结（你要记住的）

1. **copyin() 是跨页的**，所以需要循环。
2. 每次循环都判断页面是否有效（walkaddr）。
3. 使用物理地址来拷贝。
4. 不能直接用用户给的虚拟地址，因为内核不能直接 dereference 用户虚拟地址——这是安全与隔离的底线。
5. 跨页是常见的情况：
   例如用户传给系统调用一个大字符串，几 KB 或几十 KB，肯定跨多页。

---

#### 一个最常见的例子：**从用户态读取路径**

```c
copyin(p->pagetable, buf, user_path_ptr, 128);
```

可能发生：

* 用户路径跨 2~3 页
* 某一页没映射
* 引发返回 -1，让内核知道“不安全，不读了”

Xv6 系统调用基本都靠它来 “安全读取” 用户提供的数据。


sbrkarg 测试：
- 用户调用 write(fd, buf, size)
- buf 落在刚扩展但未分配物理页的区域
- copyin() 试图读用户空间 → page fault
- 你在 copyin() 里分配页面
- 若失败就 p->killed=1
- syscall 最后阶段检查 killed → exit()
- 用户态 write() 返回失败
于是 sbrkarg: write sbrk failed

---

##  测试顺序
1. 编译并启动 xv6：make qemu 或 make。
2. 在 shell 中运行简单命令：echo hi（应该至少产生一次缺页，并成功打印）。
3. 运行 lazytests：lazytests 或运行 usertests。
4. 观察具体失败用例，按日志定位（常见 log：usertrap(): unexpected scause ... stval=0x... pte=0x0）。

5. 特别用例：
- 分配后访问并 sbrk(-page) 回收检查 kfree 被调用（痕迹可在内核日志加入 cprintf）。
- fork() 后父子访问未分配页，触发各自缺页分配。
- 调用 read/write 时 copyin/copyout 能正确触发分配

## Debug for lazytests

```bash
$ lazytests
lazytests starting
running test lazy alloc
test lazy alloc: OK
running test lazy unmap
scause 0x000000000000000d
sepc=0x0000000080001de0 stval=0x0000000000000000
panic: kerneltrap


# 1) 把 sepc 地址映射回内核源码/符号 —— 找到出错函数
# 在 xv6 源树目录下，编译过后 kernel image 在 kernel/kernel
riscv64-linux-gnu-objdump -d kernel/kernel | sed -n '1,200000p' > /tmp/kernel.dis
# 在 disassembly 里查找 sepc（去掉高位 0x）
grep -n " 80001de0:" /tmp/kernel.dis -n -C3 || grep -n "1de0:" /tmp/kernel.dis -n -C3
# 或更通用：
grep -n "80001de0" /tmp/kernel.dis -n -C3 || grep -n "1de0" /tmp/kernel.dis -n -C3


2867-    80001dd6:      fffff097                auipc   ra,0xfffff
2868-    80001dda:      222080e7                jalr    546(ra) # 80000ff8 <walk>
2869-    80001dde:      84aa                    mv      s1,a0
2870:    80001de0:      6108                    ld      a0,0(a0)
2871-    80001de2:      00157793                andi    a5,a0,1
2872-    80001de6:      dfe1                    beqz    a5,80001dbe <lazy_growproc+0x78>
2873-    80001de8:      b7e9                    j       80001db2 <lazy_growproc+0x6c>


# 2) 根据 stval==0 做合理推断（最常见的 bug）
# stval==0 强烈指示“内核在读一个指针，但那个指针是 NULL”。在 lazy lab 的常见错误包括：
# 在调用 walk(p->pagetable, va, 0) 后 没有检查返回值，就做 uint64 pa = PTE2PA(*pte);（pte 为 NULL → 解引用导致 kernel fault）。
# 在 uvmunmap / lazy shrink / uvmcopy / copyin/copyout 等代码处对 pte 的错误使用。

# 3) 在怀疑点增加诊断输出（快速验证）
# 找到可疑函数（例如你之前修改过的 lazy_growproc, uvmunmap, uvmcopy, copyin, usertrap）后，在关键处加 cprintf 打印 pte 返回值和 va，示例：
pte_t *pte = walk(p->pagetable, va, 0);
if (pte == 0) {
    cprintf("DEBUG: walk returned NULL for va 0x%p in func XYZ, pid=%d\n", va, myproc()->pid);
} else {
    cprintf("DEBUG: pte for va 0x%p: 0x%p\n", va, *pte);
}
# 重新编译、运行 lazytests，看打印是否在报 panic 前出现。如果你看到 walk returned NULL 然后紧接 kerneltrap，那就基本确认了 NULL deref 问题点。

# 4) 常见具体修复（代码模式 + 解释）
# 下面是你极有可能需要做的修复模式。把这些“防护式”检查放回出问题的函数里（例如在 proc.c 的 lazy_growproc 或 uvmunmap）：
# 修复 A：在 deref 前检查 pte 是否为 NULL
pte_t *pte = walk(p->pagetable, va, 0);
if (pte == 0) {
    // 没有页表项：对于 shrink 这可能意味着 page 未映射，直接跳过
    // 或者如果在其他上下文，这可能是非法，处理为错误
    cprintf("uvmunmap: no pte for va 0x%p, ignoring\n", va);
    continue;   // 或者返回 -1 / kill
}
if((*pte & PTE_V) == 0) {
    // 未映射页面：按 lazy 语义，不需要 kfree，直接清 pte（或忽略）
    *pte = 0;
    continue;
}
// 现在安全 deref
uint64 pa = PTE2PA(*pte);
kfree((void*)pa);
*pte = 0;



```

## debug for usertests
```bash
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ lazytests
lazytests starting
running test lazy alloc
test lazy alloc: OK
running test lazy unmap
test lazy unmap: OK
running test out of memory
test out of memory: OK
ALL TESTS PASSED
$ usertests
usertests starting
usertrap: page fault with scause 15 kalloc failed , pid=76
test execout: usertrap: page fault with scause 15 kalloc failed , pid=78
usertrap: page fault with scause 15 kalloc failed , pid=79
usertrap: page fault with scause 15 kalloc failed , pid=80
usertrap: page fault with scause 15 kalloc failed , pid=81

test pgbug: OK
test sbrkbugs: usertrap(): unexpected scause 0x000000000000000c pid=3307
            sepc=0x0000000000006f0e stval=0x0000000000006f0e
usertrap(): unexpected scause 0x000000000000000c pid=3308
            sepc=0x0000000000006f0e stval=0x0000000000006f0e
OK
test badarg: OK

test bsstest: OK
test sbrkbasic: usertrap: page fault with scause 15 kalloc failed , pid=6283
OK
test sbrkmuch: OK
test kernmem: OK
test sbrkfail: usertrap: page fault with scause 13 kalloc failed , pid=6338
OK
test sbrkarg: sbrkarg: write sbrk failed
FAILED
test validatetest: OK
test stacktest: panic: remap

can you help to analyze the log on terminal and help to fix





$ usertests
usertests starting
usertrap: page fault with scause 15 kalloc failed , pid=73
test execout: usertrap: page fault with scause 15 kalloc failed , pid=75
usertrap: page fault with scause 15 kalloc failed , pid=76
usertrap: page fault with scause 15 kalloc failed , pid=77
usertrap: page fault with scause 15 kalloc failed , pid=78
usertrap: page fault with scause 15 kalloc failed , pid=79
usertrap: page fault with scause 15 kalloc failed , pid=80
usertrap: page fault with scause 15 kalloc failed , pid=81
usertrap: page fault with scause 15 kalloc failed , pid=82
usertrap: page fault with scause 15 kalloc failed , pid=83
usertrap: page fault with scause 15 kalloc failed , pid=84
usertrap: page fault with scause 15 kalloc failed , pid=85
usertrap: page fault with scause 15 kalloc failed , pid=86
usertrap: page fault with scause 15 kalloc failed , pid=87
usertrap: page fault with scause 15 kalloc failed , pid=88
usertrap: page fault with scause 15 kalloc failed , pid=89
OK
test copyin: write(fd, 0x0000000080000000, 8192) returned 8192, not -1
panic: freewalk: leaf

```
