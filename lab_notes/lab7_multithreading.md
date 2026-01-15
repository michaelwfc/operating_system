Lab: Multithreading
This lab will familiarize you with multithreading. You will implement switching between threads in a user-level threads  package, use multiple threads to speed up a program, and implement a barrier.

To start the lab, switch to the thread branch:

```bash
$ git fetch
$ git checkout thread
$ make clean
```

# Uthread: switching between threads (moderate)
In this exercise you will design the context switch mechanism for a user-level threading system, and then implement it. To get you started, your xv6 has two files `user/uthread.c` and `user/uthread_switch.S`, and a rule in the Makefile to build a uthread program. 
`uthread.c` contains most of a user-level threading package, and code for three simple test threads. The threading package is missing some of the code to create a thread and to switch between threads.

Your job is to come up with a plan to create threads and save/restore registers to switch between threads, and implement that plan. When you're done, make grade should say that your solution passes the uthread test.

## Test
Once you've finished, you should see the following output when you run uthread on xv6 (the three threads might start in a different order):
```bash
$ make qemu
...
$ uthread
thread_a started
thread_b started
thread_c started
thread_c 0
thread_a 0
thread_b 0
thread_c 1
thread_a 1
thread_b 1
...
thread_c 99
thread_a 99
thread_b 99
thread_c: exit after 100
thread_a: exit after 100
thread_b: exit after 100
thread_schedule: no runnable threads
$
```

This output comes from the three test threads, each of which has a loop that prints a line and then yields the CPU to the other threads.


At this point, however, with no context switch code, you'll see no output.


You will need to add code to `thread_create()` and `thread_schedule()` in `user/uthread.c`, and `thread_switch` in `user/uthread_switch.S`. 
- One goal is ensure that when `thread_schedule()` runs a given thread for the first time, the thread executes the function passed to `thread_create()`, on its own stack. 
- Another goal is to ensure that `thread_switch` saves the registers of the thread being switched away from, restores the registers of the thread being switched to, and returns to the point in the latter thread's instructions where it last left off. You will have to decide where to save/restore registers; modifying struct thread to hold registers is a good plan. You'll need to add a call to thread_switch in thread_schedule; you can pass whatever arguments you need to thread_switch, but the intent is to switch from thread t to next_thread.

## Some hints:

- `thread_switch` needs to save/restore only the callee-save registers. Why?
- You can see the assembly code for uthread in `user/uthread.asm`, which may be handy for debugging.

### Debugging uthread with GDB

#### gdb-multiarch kernel/kernel

调试 uthread 的正确姿势是：
- 仍然用内核作为 gdb 的入口
- 在合适的时候加载 user/_uthread 的符号

```bash
# 终端 1：启动 xv6（带 gdb）
make qemu-gdb

# 终端 2：启动 gdb
gdb-multiarch kernel/kernel
# 调试内核时 这是合理的，因为：
# QEMU 启动后
# - CPU 从 kernel/kernel 的入口地址开始执行
# gdb 知道：
# - PC 在哪
# - 内存布局是什么

# 然后在 gdb 里：
set architecture riscv:rv64
target remote :26000
add-symbol-file user/_uthread 0

b uthread.c:60
b thread_switch
c
# 当 shell 里运行 uthread 时，断点会命中。
```

- To test your code it might be helpful to single step through your `thread_switch` using `riscv64-linux-gnu-gdb`. You can get started in this way:
```bash
(gdb) file user/_uthread
Reading symbols from user/_uthread...
(gdb) b uthread.c:60



```

This sets a breakpoint at line 60 of uthread.c. The breakpoint may (or may not) be triggered before you even run uthread. How could that happen?

Once your xv6 shell runs, type "uthread", and gdb will break at line 60. 

```bash
# Now you can type commands like the following to inspect the state of uthread:
(gdb) p/x *next_thread

# With "x", you can examine the content of a memory location:
(gdb) x/x next_thread->stack

# You can skip to the start of thread_switch thus:
(gdb) b thread_switch
(gdb) c

# You can single step assembly instructions using:
(gdb) si
# On-line documentation for gdb is here.


info reg  # 看 ra / sp / s0
x/8gx $a0 # 看 old context
x/8gx $a1 # 看 new context
```


#### 调试用户程序为什么不能直接： gdb-multiarch user/_uthread
这是理解 xv6 调试模型的关键点。
原因一句话版
user/_uthread 不是一个“独立运行的程序” , 它是被 xv6 内核加载、映射、运行的。
gdb 必须附着在：
- 真正控制 CPU 的那个东西上
- 也就是 QEMU 里的 xv6 kernel

调试用户程序时，情况完全不同

```bash
# 当你在 xv6 shell 里输入：
$ uthread
```
实际发生的是：
- shell 调用 fork
- 子进程 exec("uthread")
- 内核把 ELF 加载到用户虚拟地址空间
- 设置：
  - user page table
  - user stack
  - sepc = 用户入口地址
  - sret → 跳进用户态
CPU 始终被内核控制
用户程序只是“被内核运行的代码片段”

如果你直接 gdb user/_uthread 会发生什么？
gdb 会以为：
- 这是一个 裸机程序
- 从 ELF entry 开始跑
- 但实际上：
  - QEMU 里根本没在跑它
  - 内存地址完全对不上

结果就是：
- 断点不命中
- PC 对不上
- 单步全是幻觉


那为什么 add-symbol-file user/_uthread 0 可以？
这是一个 “只加载符号，不控制执行” 的操作。
你是在告诉 gdb：
- “CPU 还是那个 CPU
- 但如果 PC 落在 user/_uthread 的地址范围
- 请用这些符号来解释它”

xv6 的简化点在于：
- 用户程序被链接成 位置无关 / 固定低地址
- gdb 可以用 0 作为基地址
- 教学系统刻意降低了复杂度

```bash

add-symbol-file user/_uthread 0


(gdb) file user/_uthread
(gdb) b uthread.c:60
```
这是一个教学上的简化说法，目的是告诉你：
- “你可以用 gdb 看 uthread 的源码和符号”
- 而不是在讲完整的 attach 流程

在真实操作中：
- 你永远是 attach 到 kernel
- 再额外挂 user 程序符号


总结
gdb 只能控制一个“真正运行的 CPU”
在 xv6 中：
- CPU 跑的是 kernel
- user 程序只是被 kernel 调度

所以：
- 始终 attach kernel
- 按需加载 user 符号

这是 Unix 调试模型的基本法则，四十年没变。


## Solution Idea

线程 ≈ 栈 + 寄存器快照 + 一次精心设计的 ret

这是 Unix、xv6、Linux、乃至所有现代 OS 的共同祖先智慧。
学会这一套，你以后看内核 scheduler、协程、fiber、甚至 async/await，都会觉得它们在用不同语言讲同一个故事。


### 整体解决思路
这是 用户态线程（green threads），不是内核线程。

线程切换 = 保存当前线程的寄存器 + 恢复另一个线程的寄存器 + ret

没有中断、没有陷入内核、没有 scheduler tick。 一切切换都发生在 普通函数调用路径 上。

你真正要解决的两个问题
1. 第一次运行一个线程时
它要： 
- 用“自己的栈”
- 从 thread_create(func) 传入的 func() 开始执行

2. 之后每次 yield
- 能从上一次停下的地方继续执行

这两个问题，都靠一件事解决： 保存 / 恢复寄存器（尤其是 sp 和 ra）

### Build a struct context as thread context

context = “下次从哪条指令 + 用哪块栈 + 保持哪些局部变量”
```c
// save the callee register as thread context
struct context {
  uint64 ra;
  uint64 sp;
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context context;
};
```
### thread_switch

类似于 kernel/swtch.S

```asm
thread_switch:
  # a0 = old context
  # a1 = new context

  # 保存 old
  sd ra, 0(a0)
  sd sp, 8(a0)
  sd s0, 16(a0)
  ...
  sd s11, 104(a0)

  # 恢复 new
  ld ra, 0(a1)
  ld sp, 8(a1)
  ld s0, 16(a1)
  ...
  ld s11, 104(a1)

  ret

```

### thread_create
thread_create 的本质 : 伪造一个“被切走过的线程上下文”

你要做三件事：
1. 找一个 FREE 的 thread
2. 给它一块栈
3. 让它第一次被调度时，看起来像是“刚从 thread_switch 返回”


1. 栈怎么放？
xv6 用户栈向下增长：
```c
uint64 sp = (uint64)t->stack + STACK_SIZE;

```

2. ra 放什么？
放线程入口函数：
```c
t->context.ra = (uint64)func;
t->context.sp = sp;
```
于是：
- 当 thread_switch 恢复这个 context
- 执行 ret
- 直接跳进 func()

这招非常古老，也非常优雅，跟内核 fork 第一跳是同一个思想。

```c
void thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE)
      break;
  }

  t->state = RUNNABLE;

  uint64 sp = (uint64)t->stack + STACK_SIZE;
  sp -= sp % 16;        // ABI 对齐（好习惯）

  t->context.sp = sp;
  t->context.ra = (uint64)func;
}

```

### thread_schedule：真正的切换点

```c
if (current_thread != next_thread) {
  next_thread->state = RUNNING;
  t = current_thread;
  current_thread = next_thread;

  thread_switch((uint64)&t->context,
                (uint64)&next_thread->context);
}

```


## Debug

### Bug 1
```bash
xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh
$ uthread
thread_a started
thread_b started
thread_c started
thread_c 0
thread_a started
thread_a 0
usertrap(): unexpected scause 0x0000000000000002 pid=3
            sepc=0x000000000000003a stval=0x0000000000000000

after implement the uthread, what is error log from usertrap() ? how to fix

```
1. 先读懂这条 usertrap 日志
scause = 0x2 是什么？
在 RISC-V 里： scause = 2 → Illegal instruction（非法指令）

sepc = 0x000000000000003a
这是什么概念？ 0x3a ≈ 58
用户程序的代码段地址一般在 0x1000 以上
0x3a 这种地址： 不是函数, 不是栈 ,  不是映射内存 
一句话： ret 跳到了完全错误的地址

2. 结合现象：thread_a started 被打印了两次
这件事理论上不可能发生。为什么？
thread_a() 的第一行就是： printf("thread_a started\n");

正确的上下文切换下：
- 它只会在“第一次被调度”时执行
- 后续 yield 再回来，应该从 thread_yield() 之后继续

但现在它又从头执行了一遍。这说明什么？
thread_a 的 ra / sp 没有被正确保存或恢复
CPU 以为它是“第一次运行”

3. 核心结论：这是一个典型的“上下文被破坏”错误
综合三点证据：
- scause = 2（非法指令）
- sepc = 0x3a（极低地址）
- thread_a started 被打印两次

可以非常肯定地说：
thread_switch 保存 / 恢复 context 有 bug 或者  thread_create 初始化 context 有 bug

### Debug
```bash
(gdb) where
#0  thread_init () at user/uthread.c:52
#1  0x000000000000052c in main (argc=1, argv=0xbfe0) at user/uthread.c:184
#2  0x000000000000008c in thread_schedule () at user/uthread.c:68
Backtrace stopped: previous frame inner to this frame (corrupt stack?)


(gdb) p &all_thread
$9 = (struct thread (*)[4]) 0x1408 <all_thread>

# current_thread = &all_thread[0];
(gdb) p all_thread[0]
$5 = {
  stack =     '\000' <repeats 8191 times>,
  state = 0,
  context = {
    ra = 0,
    sp = 0,
    s0 = 0,
    s1 = 0,
    s2 = 0,
    s3 = 0,
    s4 = 0,
    s5 = 0,
    s6 = 0,
    s7 = 0,
    s8 = 0,
    s9 = 0,
    s10 = 0,
    s11 = 0
  }
}

(gdb) p current_thread
$8 = (struct thread *) 0x1408 <all_thread>


(gdb) bt
#0  thread_create (func=0x220 <thread_a>) at user/uthread.c:98
#1  0x000000000000053c in main (argc=1, argv=0xbfe0) at user/uthread.c:185
#2  0x000000000000008c in thread_schedule () at user/uthread.c:68
Backtrace stopped: previous frame inner to this frame (corrupt stack?)

(gdb) p t
$10 = (struct thread *) 0x1408 <all_thread>

(gdb) p *t
$14 = {
  stack =     '\000' <repeats 8191 times>,
  state = 1,
  context = {
    ra = 0,
    sp = 0,
    s0 = 0,
    s1 = 0,
    s2 = 0,
    s3 = 0,
    s4 = 0,
    s5 = 0,
    s6 = 0,
    s7 = 0,
    s8 = 0,
    s9 = 0,
    s10 = 0,
    s11 = 0
  }
}


# t->state = RUNNABLE;
(gdb) p *t
$17 = {
  stack =     '\000' <repeats 8191 times>,
  state = 2,
  context = {
    ra = 0,
    sp = 0,
    s0 = 0,
    s1 = 0,
    s2 = 0,
    s3 = 0,
    s4 = 0,
    s5 = 0,
    s6 = 0,
    s7 = 0,
    s8 = 0,
    s9 = 0,
    s10 = 0,
    s11 = 0
  }
}

(gdb) p/x t->stack
$21 =   {0x0 <repeats 8192 times>}
(gdb) p/x sp
$22 = 0x5480
(gdb) next
=> 0x00000000000001b4 <thread_create+110>:      03 37 84 fe     ld      a4,-24(s0)
   0x00000000000001b8 <thread_create+114>:      89 67   lui     a5,0x2
   0x00000000000001ba <thread_create+116>:      a1 07   addi    a5,a5,8
   0x00000000000001bc <thread_create+118>:      ba 97   add     a5,a5,a4
(gdb) p/x sp
$23 = 0x5480

(gdb) p func
$24 = (void (*)()) 0x220 <thread_a>


(gdb) bt
#0  thread_switch () at user/uthread_switch.S:10
#1  0x0000000000000136 in thread_schedule () at user/uthread.c:87
#2  0x0000000000000564 in main (argc=1, argv=0xbfe0) at user/uthread.c:188
#3  0x000000000000008c in thread_schedule () at user/uthread.c:68
Backtrace stopped: previous frame inner to this frame (corrupt stack?)


```


### Test again

```bash

xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh
$ uthread
thread_a started
thread_b started
thread_c started
thread_c 0
thread_a 0
thread_b 0
thread_c 1
thread_a 1
thread_b 1
thread_c 2
thread_a 2
thread_b 2
thread_c 3
thread_a 3
thread_b 3
...
thread_c 99
thread_a 99
thread_b 99
thread_c: exit after 100
thread_a: exit after 100
thread_b: exit after 100
thread_schedule: no runnable threads
```

# Using threads (moderate)

In this assignment you will explore parallel programming with threads and locks using a hash table. You should do this assignment on a real Linux or MacOS computer (not xv6, not qemu) that has multiple cores. Most recent laptops have multicore processors.

This assignment uses the `UNIX pthread threading library`. You can find information about it from the manual page, with  `man pthreads`, and you can look on the web, for example 
- [here](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_mutex_lock.html) 
- [here](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_mutex_init.html)
- [here](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_create.html).

The file `notxv6/ph.c` contains a simple hash table that is correct if used from a single thread, but incorrect when used from multiple threads. In your main xv6 directory (perhaps ~/xv6-labs-2020), type this:

```bash
$ make ph
$ ./ph 1
```

Note that to build `ph` the Makefile uses your OS's gcc, not the 6.S081 tools. The argument to ph specifies the number of threads that execute put and get operations on the the hash table. After running for a little while, ph 1 will produce output similar to this:

```bash
100000 puts, 3.991 seconds, 25056 puts/second
0: 0 keys missing
100000 gets, 3.981 seconds, 25118 gets/second
```

The numbers you see may differ from this sample output by a factor of two or more, depending on how fast your computer is, whether it has multiple cores, and whether it's busy doing other things.

ph runs two benchmarks. 
- First it adds lots of keys to the hash table by calling put(), and prints the achieved rate in puts per second. 
- Then it fetches keys from the hash table with get(). It prints the number keys that should have been in the hash table as a result of the puts but are missing (zero in this case), and it prints the number of gets per second it achieved.

You can tell ph to use its hash table from multiple threads at the same time by giving it an argument greater than one. 
Try ph 2:

```bash
$ ./ph 2
100000 puts, 1.885 seconds, 53044 puts/second
1: 16579 keys missing
0: 16579 keys missing
200000 gets, 4.322 seconds, 46274 gets/second

```
The first line of this ph 2 output indicates that when two threads concurrently add entries to the hash table, they achieve a total rate of 53,044 inserts per second. That's about twice the rate of the single thread from running ph 1. That's an excellent "parallel speedup" of about 2x, as much as one could possibly hope for (i.e. twice as many cores yielding twice as much work per unit time).
However, the two lines saying 16579 keys missing indicate that a large number of keys that should have been in the hash table are not there. That is, the puts were supposed to add those keys to the hash table, but something went wrong. Have a look at `notxv6/ph.c`, particularly at `put()` and `insert()`.

Why are there missing keys with 2 threads, but not with 1 thread? 
Identify a sequence of events with 2 threads that can lead to a key being missing. Submit your sequence with a short explanation in answers-thread.txt
To avoid this sequence of events, insert `lock` and `unlock` statements in put and get in notxv6/ph.c so that the number of keys missing is always 0 with two threads. The relevant pthread calls are:

```bash
pthread_mutex_t lock;            // declare a lock
pthread_mutex_init(&lock, NULL); // initialize the lock
pthread_mutex_lock(&lock);       // acquire lock
pthread_mutex_unlock(&lock);     // release lock
```

You're done when make grade says that your code passes the `ph_safe` test, which requires zero missing keys with two threads. It's OK at this point to fail the `ph_fast` test.

Don't forget to call `pthread_mutex_init()`. Test your code first with 1 thread, then test it with 2 threads. Is it correct (i.e. have you eliminated missing keys?)? Does the two-threaded version achieve parallel speedup (i.e. more total work per unit time) relative to the single-threaded version?

There are situations where concurrent put()s have no overlap in the memory they read or write in the hash table, and thus don't need a lock to protect against each other. Can you change ph.c to take advantage of such situations to obtain parallel speedup for some put()s? 

Hint: how about a lock per hash bucket?

Modify your code so that some put operations run in parallel while maintaining correctness. You're done when make grade says your code passes both the `ph_safe` and `ph_fast` tests. 
The `ph_fast` test requires that two threads yield at least 1.25 times as many puts/second as one thread.


## UNIX pthread threading library

pthread（POSIX threads）就是这套约定在 UNIX 世界里的官方版本。

一、pthread 到底是什么？
pthread = POSIX Threads
POSIX：Portable Operating System Interface
一套为了让 UNIX 系统“长得一样”的标准
Threads：线程 API 规范
Linux、macOS、FreeBSD、Solaris…… 都实现了它（实现细节不同，但接口一样）。




# Barrier(moderate)

In this assignment you'll implement a `barrier`(http://en.wikipedia.org/wiki/Barrier_(computer_science)): a point in an application at which all participating threads must wait until all other participating threads reach that point too. You'll use pthread condition variables, which are a sequence coordination technique similar to xv6's sleep and wakeup.

You should do this assignment on a real computer (not xv6, not qemu).

The file notxv6/barrier.c contains a broken barrier.
```bash
$ make barrier
$ ./barrier 2
barrier: notxv6/barrier.c:42: thread: Assertion `i == t' failed.
```
The 2 specifies the number of threads that synchronize on the barrier ( nthread in barrier.c). Each thread executes a loop. In each loop iteration a thread calls barrier() and then sleeps for a random number of microseconds. The assert triggers, because one thread leaves the barrier before the other thread has reached the barrier. The desired behavior is that each thread blocks in barrier() until all nthreads of them have called barrier().

Your goal is to achieve the desired barrier behavior. In addition to the lock primitives that you have seen in the ph assignment, you will need the following new pthread primitives; look here and here for details.

```bash
pthread_cond_wait(&cond, &mutex);  // go to sleep on cond, releasing lock mutex, acquiring upon wake up
pthread_cond_broadcast(&cond);     // wake up every thread sleeping on cond
```
Make sure your solution passes make grade's barrier test.

`pthread_cond_wait` releases the mutex when called, and re-acquires the mutex before returning.
We have given you `barrier_init()`. Your job is to implement `barrier()` so that the panic doesn't occur. We've defined struct barrier for you; its fields are for your use.

There are two issues that complicate your task:

- You have to deal with a succession of barrier calls, each of which we'll call a round. 
  `bstate.round` records the current round. 
  You should increment `bstate.round` each time all threads have reached the barrier.

- You have to handle the case in which one thread races around the loop before the others have exited the barrier. 
  In particular, you are re-using the `bstate.nthread` variable from one round to the next. 
  Make sure that a thread that leaves the barrier and races around the loop doesn't increase `bstate.nthread` while a previous round is still using it.
  
Test your code with one, two, and more than two threads.

## What is a barrier?

Barrier = “集结点”

所有线程必须先到齐， 才允许任何一个继续往下走。





### 为什么说 barrier 类似 xv6 的 sleep / wakeup？

sleep(chan, lock);
wakeup(chan);

语义是：

条件不满足 → sleep

条件满足 → wakeup 所有人

sleep 会原子地释放锁并进入等待

wakeup 不关心“是谁”，只关心“条件到了”

这跟 barrier 的逻辑 完全同构。

### pthread_cond_wait 是 sleep 的现代版
pthread_cond_wait(&cond, &mutex); 
在语义上等价于 xv6 的： 
sleep(chan, lock);


### 正确的 barrier 心智模型（非常重要）

每一轮 barrier，都必须满足这三件事：
1. 每个线程记住： “我进入 barrier 时的 round 是多少”
2. 如果： 到达线程数 < nthreads 就 sleep
3. 最后一个线程： 
round++
nthread = 0
broadcast

而等待者醒来后： 不是立刻走, 而是检查： round 是否已经变化

这正是 condition variable 的标准用法：

while (condition_not_met)
    pthread_cond_wait(&cond, &mutex);
