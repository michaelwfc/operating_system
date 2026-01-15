# 7.1 Multiplexing

Xv6 multiplexes by switching each CPU from one process to another in two situations.

1. xv6’s sleep and wakeup mechanism switches when
   - a process waits for device
   - or pipe I/O to complete,
   - or waits for a child to exit,
   - or waits in the sleep system call.
2. xv6 periodically forces a switch to cope with processes that compute for long periods without sleeping.

This multiplexing creates the illusion that each process has its own CPU, much as xv6 uses the memory allocator and hardware page tables to create the illusion that each process has its own memory.

Implementing multiplexing poses a few challenges.

1. how to switch from one process to another?
   Although the idea of context switching is simple, the implementation is some of the most opaque code in xv6.
2. how to force switches in a way that is transparent to user processes?
   Xv6 uses the standard technique in which a hardware timer’s interrupts drive context switches.
3. all of the CPUs switch among the same shared set of processes, and a locking plan is necessary to avoid races.

4. a process’s memory and other resources must be freed when the process exits, but it cannot do all of this itself because (for example) it can’t free its own kernel stack while still using it.
5. each core of a multi-core machine must remember which process it is executing so that system calls affect the correct process’s kernel state.
6. sleep and wakeup allow a process to give up the CPU and wait to be woken up by another process or
   interrupt.
   Care is needed to avoid races that result in the loss of wakeup notifications. Xv6 tries to
   solve these problems as simply as possible, but nevertheless the resulting code is tricky.

# 7.2 Code: Context switching

Figure 7.1 outlines the steps involved in switching from one user process to another:

- a user-kernel transition (system call or interrupt) to the old process’s kernel thread
- a context switch to the current CPU’s scheduler thread
- a context switch to a new process’s kernel thread, and
- a trap return to the user-level process.

The xv6 scheduler has a dedicated thread (saved registers and stack) per CPU because it is not safe for the scheduler execute on the old process’s kernel stack: some other core might wake the process up and run it, and it would be a disaster to use the same stack on two different cores.

In this section we’ll examine the mechanics of switching between a **kernel thread** and a **scheduler thread**.

Switching from one thread to another involves saving the old thread’s CPU registers, and restoring the previously-saved registers of the new thread; the fact that the `stack pointer` and `program counter` are saved and restored means that the CPU will switch stacks and switch what code it is executing.

# Thread

## 1. 线程（Thread）概述

我们今天的课程会讨论线程以及 XV6 如何实现线程切换。今天这节课与之前介绍的系统调用，Interrupt，page table 和锁的课程一样，都是有关 XV6 底层实现的课程。今天我们将讨论 XV6 如何在多个线程之间完成切换。

为什么计算机需要运行多线程？可以归结为以下原因：

- 首先，人们希望他们的计算机在同一时间不是只执行一个任务。
- 有可能计算机需要执行 **分时复用** 的任务，例如 MIT 的公共计算机系统 Athena 允许多个用户同时登陆一台计算机，并运行各自的进程。甚至在一个单用户的计算机或者在你的 iphone 上，你会运行多个进程，并期望计算机完成所有的任务而不仅仅只是一个任务。

- 其次，多线程可以让程序的结构变得简单。
  线程在有些场合可以帮助程序员将代码以简单优雅的方式进行组织，并减少复杂度。实际上在第一个 lab 中 prime number 部分，通过多个进程可以更简单，方便，优雅的组织代码。

- 最后，使用多线程可以通过并行运算，在拥有多核 CPU 的计算机上获得更快的处理速度。
  常见的方式是将程序进行拆分，并通过线程在不同的 CPU 核上运行程序的不同部分。如果你足够幸运的话，你可以将你的程序拆分并在 4 个 CPU 核上通过 4 个线程运行你的程序，同时你也可以获取 4 倍的程序运行速度。你可以认为 XV6 就是一个多 CPU 并行运算的程序。

### Thread: One serial excectuion

所以，线程可以认为是一种在有多个任务时简化编程的抽象。一个线程可以认为是**串行执行代码的单元**。
如果你写了一个程序只是按顺序执行代码，那么你可以认为这个程序就是个单线程程序，这是对于线程的一种宽松的定义。虽然人们对于线程有很多不同的定义，在这里，我们认为线程就是单个串行执行代码的单元，它只占用一个 CPU 并且以普通的方式一个接一个的执行指令。

除此之外，线程还具有**状态**，我们可以随时保存线程的状态并暂停线程的运行，并在之后通过恢复状态来恢复线程的运行。线程的状态包含了三个部分：

- `Program Counter`: 程序计数器它表示当前线程执行指令的位置。
- `Registers`: 保存变量的寄存器。
- `程序的Stack`（注，详见 5.5）。通常来说每个线程都有属于自己的 Stack， Stack 记录了函数调用的记录，并反映了当前线程的执行点。

操作系统中线程系统的工作就是管理多个线程的运行。我们可能会启动成百上千个线程，而线程系统的工作就是弄清楚如何管理这些线程并让它们都能运行。

### 多线程的并行运行主要有两个策略：

1. 在多核处理器上使用多个 CPU
   每个 CPU 都可以运行一个线程，如果你有 4 个 CPU，那么每个 CPU 可以运行一个线程。每个线程自动的根据所在 CPU 就有了程序计数器和寄存器。但是如果你只有 4 个 CPU，却有上千个线程，每个 CPU 只运行一个线程就不能解决这里的问题了。

2. 一个 CPU 在多个线程之间来回切换。
   假设我只有一个 CPU，但是有 1000 个线程，我们接下来将会看到 XV6 是如何实现线程切换使得 XV6 能够先运行一个线程，之后将线程的状态保存，再切换至运行第二个线程，然后再是第三个线程，依次类推直到每个线程都运行了一会，再回来重新执行第一个线程。

实际上，与大多数其他操作系统一样，XV6 结合了这两种策略，首先线程会运行在所有可用的 CPU 核上，其次每个 CPU 核会在多个线程之间切换，因为通常来说，线程数会远远多于 CPU 的核数。

### Sharing memeory

不同线程系统之间的一个主要的区别就是，线程之间是否会共享内存。一种可能是你有一个地址空间，多个线程都在这一个地址空间内运行，并且它们可以看到彼此的更新。比如说共享一个地址空间的线程修改了一个变量，共享地址空间的另一个线程可以看到变量的修改。所以当多个线程运行在一个共享地址空间时，我们需要用到上节课讲到的锁。

#### xv6 kernel threads - Yes

XV6 内核共享了内存，并且 XV6 支持内核线程的概念，对于每个用户进程都有一个内核线程来执行来自用户进程的系统调用。所有的内核线程都共享了内核内存，所以 XV6 的内核线程的确会共享内存。

#### xv6 user threads - No

另一方面，XV6 还有另外一种线程。每一个用户进程都有独立的内存地址空间（注，详见 4.2），并且包含了一个线程，这个线程控制了用户进程代码指令的执行。所以 XV6 中的用户线程之间没有共享内存，你可以有多个用户进程，但是每个用户进程都是拥有一个线程的独立地址空间。XV6 中的进程不会共享内存。

### Linux user threads - Yes

在一些其他更加复杂的系统中，例如 Linux，允许在一个用户进程中包含多个线程，进程中的多个线程共享进程的地址空间。当你想要实现一个运行在多个 CPU 核上的用户进程时，你就可以在用户进程中创建多个线程。Linux 中也用到了很多我们今天会介绍的技术，但是在 Linux 中跟踪每个进程的多个线程比 XV6 中每个进程只有一个线程要复杂的多。

还有一些其他的方式可以支持在一台计算机上交织的运行多个任务，我们不会讨论它们，但是如果你感兴趣的话，你可以去搜索 event-driven programming 或者 state machine，这些是在一台计算机上不使用线程但又能运行多个任务的技术。在所有的支持多任务的方法中，线程技术并不是非常有效的方法，但是线程通常是最方便，对程序员最友好的，并且可以用来支持大量不同任务的方法。

## 2. XV6 线程调度

实现内核中的线程系统存在以下挑战：

1. 如何实现线程间的切换。
   这里停止一个线程的运行并启动另一个线程的过程通常被称为线程调度（Scheduling）。我们将会看到 XV6 为每个 CPU 核都创建了一个线程调度器（Scheduler）。

2. 当你想要实际实现从一个线程切换到另一个线程时，你需要保存并恢复线程的状态，所以需要决定线程的哪些信息是必须保存的，并且在哪保存它们。

3. 如何处理运算密集型线程（compute bound thread）。
   对于线程切换，很多直观的实现是由线程自己自愿的保存自己的状态，再让其他的线程运行。但是如果我们有一些程序正在执行一些可能要花费数小时的长时间计算任务，这样的线程并不能自愿的出让 CPU 给其他的线程运行。所以这里需要能从长时间运行的运算密集型线程撤回对于 CPU 的控制，将其放置于一边，稍后再运行它。

### 运算密集型线程（compute bound thread）

接下来，我将首先介绍如何处理运算密集型线程。这里的具体实现你们之前或许已经知道了，就是利用定时器中断。在每个 CPU 核上，都存在一个硬件设备，它会定时产生中断。XV6 与其他所有的操作系统一样，将这个中断传输到了内核中。所以即使我们正在用户空间计算 π 的前 100 万位，定时器中断仍然能在例如每隔 10ms 的某个时间触发，并将程序运行的控制权从用户空间代码切换到内核中的中断处理程序（注，因为中断处理程序优先级更高）。哪怕这些用户空间进程并不配合工作（注，也就是用户空间进程一直占用 CPU），内核也可以从用户空间进程获取 CPU 控制权。

位于内核的定时器中断处理程序，会自愿的将 CPU 出让（yield）给线程调度器，并告诉线程调度器说，你可以让一些其他的线程运行了。这里的出让其实也是一种线程切换，它会保存当前线程的状态，并在稍后恢复。

```
Timer interrupts
Kernel Handler
 yields -> switch
```

在之前的课程中，你们已经了解过了中断处理的流程。这里的基本流程是，定时器中断将 CPU 控制权给到内核，内核再自愿的出让 CPU。

#### Pre-emptive scheduling

这样的处理流程被称为 pre-emptive scheduling。pre-emptive 的意思是，即使用户代码本身没有出让 CPU，定时器中断仍然会将 CPU 的控制权拿走，并出让给线程调度器。与之相反的是 voluntary scheduling。

有趣的是，在 XV6 和其他的操作系统中，线程调度是这么实现的：定时器中断会强制的将 CPU 控制权从用户进程给到内核，这里是 pre-emptive scheduling，之后内核会代表用户进程（注，实际是内核中用户进程对应的内核线程会代表用户进程出让 CPU），使用 voluntary scheduling。

在执行线程调度的时候，操作系统需要能区分几类线程：

- 当前在 CPU 上运行的线程
- 一旦 CPU 有空闲时间就想要运行在 CPU 上的线程
- 以及不想运行在 CPU 上的线程，因为这些线程可能在等待 I/O 或者其他事件

这里不同的线程是由状态区分，但是实际上线程的完整状态会要复杂的多（注，线程的完整状态包含了程序计数器，寄存器，栈等等）。下面是我们将会看到的一些线程状态：

- RUNNING，线程当前正在某个 CPU 上运行
- RUNABLE，线程还没有在某个 CPU 上运行，但是一旦有空闲的 CPU 就可以运行
- SLEEPING，这节课我们不会介绍，下节课会重点介绍，这个状态意味着线程在等待一些 I/O 事件，它只会在 I/O 事件发生了之后运行

今天这节课，我们主要关注 RUNNING 和 RUNABLE 这两类线程。前面介绍的定时器中断或者说 pre-emptive scheduling，实际上就是将一个 RUNNING 线程转换成一个 RUNABLE 线程。通过出让 CPU，pre-emptive scheduling 将一个正在运行的线程转换成了一个当前不在运行但随时可以再运行的线程。因为当定时器中断触发时，这个线程还在好好的运行着。

对于 RUNNING 状态下的线程，它的程序计数器和寄存器位于正在运行它的 CPU 硬件中。

而 RUNABLE 线程，因为并没有 CPU 与之关联，所以对于每一个 RUNABLE 线程，当我们将它从 RUNNING 转变成 RUNABLE 时，我们需要将它还在 RUNNING 时位于 CPU 的状态拷贝到内存中的某个位置，注意这里不是从内存中的某处进行拷贝，而是从 CPU 中的寄存器拷贝。我们需要拷贝的信息就是程序计数器（Program Counter）和寄存器。

当线程调度器决定要运行一个 RUNABLE 线程时，这里涉及了很多步骤，但是其中一步是将之前保存的程序计数器和寄存器拷贝回调度器对应的 CPU 中。

## 3 XV6 线程切换

接下来我将通过两张图来介绍 XV6 中的线程切换是如何实现的，其中一张图是简单的，另一张图包含了更多的细节，这一小节先看简单的图。

我们或许会运行多个用户空间进程，例如 C compiler（CC），LS，Shell，它们或许会，也或许不会想要同时运行。在用户空间，每个进程有自己的内存，对于我们这节课来说，我们更关心的是每个进程都包含了一个用户程序栈（user stack），并且当进程运行的时候，它在 RISC-V 处理器中会有程序计数器和寄存器。当用户程序在运行时，实际上是用户进程中的一个用户线程在运行。

如果程序执行了一个系统调用或者因为响应中断走到了内核中，那么相应的用户空间状态会被保存在程序的 trapframe 中（注，详见 lec06），同时属于这个用户程序的内核线程被激活。

- 所以首先，用户的程序计数器，寄存器等等被保存到了 trapframe 中，
- 之后 CPU 被切换到内核栈上运行，实际上会走到 trampoline 和 usertrap 代码中（注，详见 lec06）。
- 之后内核会运行一段时间处理系统调用或者执行中断处理程序。
- 在处理完成之后，如果需要返回到用户空间，trapframe 中保存的用户进程状态会被恢复。

```
    CC             LS       Shell      User

User Stack
PC + Regs

TF

Kernel Stack


```

除了系统调用，用户进程也有可能是因为 CPU 需要响应类似于定时器中断走到了内核空间。上一节提到的 pre-emptive scheduling，会通过定时器中断将 CPU 运行切换到另一个用户进程。在定时器中断程序中，如果 XV6 内核决定从一个用户进程切换到另一个用户进程，那么首先在内核中第一个进程的内核线程会被切换到第二个进程的内核线程。之后再在第二个进程的内核线程中返回到用户空间的第二个进程，这里返回也是通过恢复 trapframe 中保存的用户进程状态完成。

当 XV6 从 CC 程序的内核线程切换到 LS 程序的内核线程时：

1. XV6 会首先会将 CC 程序的内核线程的内核寄存器保存在一个 context 对象中。

2. 类似的，因为要切换到 LS 程序的内核线程，那么 LS 程序现在的状态必然是 RUNABLE，表明 LS 程序之前运行了一半。这同时也意味着 LS 程序的用户空间状态已经保存在了对应的 trapframe 中，更重要的是，LS 程序的内核线程对应的内核寄存器也已经保存在对应的 context 对象中。所以接下来，XV6 会恢复 LS 程序的内核线程的 context 对象，也就是恢复内核线程的寄存器。

3. 之后 LS 会继续在它的内核线程栈上，完成它的中断处理程序（注，假设之前 LS 程序也是通过定时器中断触发的 pre-emptive scheduling 进入的内核）。

4. 然后通过恢复 LS 程序的 trapframe 中的用户进程状态，返回到用户空间的 LS 程序中。

5. 最后恢复执行 LS。

这里核心点在于，在 XV6 中，任何时候都需要经历：

1. 从一个用户进程切换到另一个用户进程，都需要从第一个用户进程接入到内核中，保存用户进程的状态并运行第一个用户进程的内核线程。
2. 再从第一个用户进程的内核线程切换到第二个用户进程的内核线程。
3. 之后，第二个用户进程的内核线程暂停自己，并恢复第二个用户进程的用户寄存器。
4. 最后返回到第二个用户进程继续执行。

这么曲折的一个线路。

### Full Process

实际的线程切换流程会复杂的多。

假设我们有进程 P1 正在运行，进程 P2 是 RUNABLE 当前并不在运行。假设在 XV6 中我们有 2 个 CPU 核，这意味着在硬件层面我们有 CPU0 和 CPU1。

我们从一个正在运行的用户空间进程切换到另一个 RUNABLE 但是还没有运行的用户空间进程的更完整的故事是：

1. 首先与我之前介绍的一样，一个定时器中断强迫 CPU 从用户空间进程切换到内核，trampoline 代码将用户寄存器保存于用户进程对应的 trapframe 对象中；

2. 之后在内核中运行 usertrap，来实际执行相应的中断处理程序。这时，CPU 正在进程 P1 的内核线程和内核栈上，执行内核中普通的 C 代码；

3. 假设进程 P1 对应的内核线程决定它想出让 CPU，它会做很多工作，这个我们稍后会看，但是最后它会调用 swtch 函数（译注：switch 是 C 语言关键字，因此这个函数命名为 swtch 来避免冲突），这是整个线程切换的核心函数之一；

4. swtch 函数会保存用户进程 P1 对应内核线程的寄存器至 context 对象。所以目前为止有两类寄存器：用户寄存器存在 trapframe 中，内核线程的寄存器存在 context 中。

但是，实际上 swtch 函数并不是直接从一个内核线程切换到另一个内核线程。XV6 中，一个 CPU 上运行的内核线程可以直接切换到的是这个 CPU 对应的调度器线程。所以如果我们运行在 CPU0，swtch 函数会恢复之前为 CPU0 的调度器线程保存的寄存器和 stack pointer，之后就在调度器线程的 context 下执行 schedulder 函数中（注，后面代码分析有介绍）。

在 schedulder 函数中会做一些清理工作，例如将进程 P1 设置成 RUNABLE 状态。之后再通过进程表单找到下一个 RUNABLE 进程。假设找到的下一个进程是 P2（虽然也有可能找到的还是 P1），schedulder 函数会再次调用 swtch 函数，完成下面步骤：

1. 先保存自己的寄存器到调度器线程的 context 对象

2. 找到进程 P2 之前保存的 context，恢复其中的寄存器

3. 因为进程 P2 在进入 RUNABLE 状态之前，如刚刚介绍的进程 P1 一样，必然也调用了 swtch 函数。所以之前的 swtch 函数会被恢复，并返回到进程 P2 所在的系统调用或者中断处理程序中（注，因为 P2 进程之前调用 swtch 函数必然在系统调用或者中断处理程序中）。

4. 不论是系统调用也好中断处理程序也好，在从用户空间进入到内核空间时会保存用户寄存器到 trapframe 对象。所以当内核程序执行完成之后，trapframe 中的用户寄存器会被恢复。

5. 最后用户进程 P2 就恢复运行了。

每一个 CPU 都有一个完全不同的调度器线程。调度器线程也是一种内核线程，它也有自己的 context 对象。任何运行在 CPU1 上的进程，当它决定出让 CPU，它都会切换到 CPU1 对应的调度器线程，并由调度器线程切换到下一个进程。

学生提问：context 保存在哪？

Robert 教授：每一个内核线程都有一个 context 对象。但是内核线程实际上有两类。每一个用户进程有一个对应的内核线程，它的 context 对象保存在用户进程对应的 proc 结构体中。

每一个调度器线程，它也有自己的 context 对象，但是它却没有对应的进程和 proc 结构体，所以调度器线程的 context 对象保存在 cpu 结构体中。在内核中，有一个 cpu 结构体的数组，每个 cpu 结构体对应一个 CPU 核，每个结构体中都有一个 context 字段。

学生提问：为什么不能将 context 对象保存在进程对应的 trapframe 中？

Robert 教授：context 可以保存在 trapframe 中，因为每一个进程都只有一个内核线程对应的一组寄存器，我们可以将这些寄存器保存在任何一个与进程一一对应的数据结构中。对于每个进程来说，有一个 proc 结构体，有一个 trapframe 结构体，所以我们可以将 context 保存于 trapframe 中。但是或许出于简化代码或者让代码更清晰的目的，trapframe 还是只包含进入和离开内核时的数据。而 context 结构体中包含的是在内核线程和调度器线程之间切换时，需要保存和恢复的数据。

学生提问：出让 CPU 是由用户发起的还是由内核发起的？

Robert 教授：对于 XV6 来说，并不会直接让用户线程出让 CPU 或者完成线程切换，而是由内核在合适的时间点做决定。有的时候你可以猜到特定的系统调用会导致出让 CPU，例如一个用户进程读取 pipe，而它知道 pipe 中并不能读到任何数据，这时你可以预测读取会被阻塞，而内核在等待数据的过程中会运行其他的进程。

内核会在两个场景下出让 CPU。当定时器中断触发了，内核总是会让当前进程出让 CPU，因为我们需要在定时器中断间隔的时间点上交织执行所有想要运行的进程。另一种场景就是任何时候一个进程调用了系统调用并等待 I/O，例如等待你敲入下一个按键，在你还没有按下按键时，等待 I/O 的机制会触发出让 CPU。

学生提问：用户进程调用 sleep 函数是不是会调用某个系统调用，然后将用户进程的信息保存在 trapframe，然后触发进程切换，这时就不是定时器中断决定，而是用户进程自己决定了吧？

Robert 教授：如果进程执行了 read 系统调用，然后进入到了内核中。而 read 系统调用要求进程等待磁盘，这时系统调用代码会调用 sleep，而 sleep 最后会调用 swtch 函数。swtch 函数会保存内核线程的寄存器到进程的 context 中，然后切换到对应 CPU 的调度器线程，再让其他的线程运行。这样在当前线程等待磁盘读取结束时，其他线程还能运行。所以，这里的流程除了没有定时器中断，其他都一样，只是这里是因为一个系统调用需要等待 I/O（注，感觉答非所问）

学生提问：每一个 CPU 的调度器线程有自己的栈吗？

Robert 教授：是的，每一个调度器线程都有自己独立的栈。实际上调度器线程的所有内容，包括栈和 context，与用户进程不一样，都是在系统启动时就设置好了。如果你查看 XV6 的 start.s（注：是 entry.S 和 start.c）文件，你就可以看到为每个 CPU 核设置好调度器线程。

这里有一个术语需要解释一下。当人们在说 context switching，他们通常说的是从一个线程切换到另一个线程，因为在切换的过程中需要先保存前一个线程的寄存器，然后再恢复之前保存的后一个线程的寄存器，这些寄存器都是保存在 context 对象中。在有些时候，context switching 也指从一个用户进程切换到另一个用户进程的完整过程。偶尔你也会看到 context switching 是指从用户空间和内核空间之间的切换。对于我们这节课来说，context switching 主要是指一个内核线程和调度器线程之间的切换。

这里有一些有用的信息可以记住。每一个 CPU 核在一个时间只会做一件事情，每个 CPU 核在一个时间只会运行一个线程，它要么是运行用户进程的线程，要么是运行内核线程，要么是运行这个 CPU 核对应的调度器线程。所以在任何一个时间点，CPU 核并没有做多件事情，而是只做一件事情。线程的切换创造了多个线程同时运行在一个 CPU 上的假象。类似的每一个线程要么是只运行在一个 CPU 核上，要么它的状态被保存在 context 中。线程永远不会运行在多个 CPU 核上，线程要么运行在一个 CPU 核上，要么就没有运行。

在 XV6 的代码中，context 对象总是由 swtch 函数产生，所以 context 总是保存了内核线程在执行 swtch 函数时的状态。当我们在恢复一个内核线程时，对于刚恢复的线程所做的第一件事情就是从之前的 swtch 函数中返回（注，有点抽象，后面有代码分析）。

学生提问：我们这里一直在说线程，但是从我看来 XV6 的实现中，一个进程就只有一个线程，有没有可能一个进程有多个线程？

Robert 教授：我们这里的用词的确有点让人混淆。在 XV6 中，一个进程要么在用户空间执行指令，要么是在内核空间执行指令，要么它的状态被保存在 context 和 trapframe 中，并且没有执行任何指令。这里该怎么称呼它呢？你可以根据自己的喜好来称呼它，对于我来说，每个进程有两个线程，一个用户空间线程，一个内核空间线程，并且存在限制使得一个进程要么运行在用户空间线程，要么为了执行系统调用或者响应中断而运行在内核空间线程 ，但是永远也不会两者同时运行。

# XV6 进程切换示例程序

接下来，我们切换到代码并展示一下刚刚介绍的内容。

## mycpu and myproc

### struct proc(kernel/proc.h)

我们先来看一下 proc.h 中的 proc 结构体，从结构体中我们可以看到很多之前介绍的内容。

```c

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};
```

- trapframe: 保存了用户空间线程寄存器(user level registers) 的 trapframe 字段
- context: 其次是保存了内核线程寄存器的 context 字段
- kstack: 还有保存了当前进程的内核栈的 kstack 字段，这是进程在内核中执行时保存函数调用的位置
- state: 保存了当前进程状态，要么是 RUNNING，要么是 RUNABLE，要么是 SLEEPING 等等
- lock: 保护了很多数据，目前来说至少保护了对于 state 字段的更新。举个例子，因为有锁的保护，两个 CPU 的调度器线程不会同时拉取同一个 RUNABLE 进程并运行它

我接下来会运行一个简单的演示程序，在这个程序中我们会从一个进程切换到另一个。

Xv6 often needs a pointer to the current process’s `proc structure`. On a uniprocessor one could have a global variable pointing to the current proc. This doesn’t work on a multi-core machine, since each core executes a different process. The way to solve this problem is to exploit the fact that **each core has its own set of registers**; we can use one of those registers to help find per-core information.

### struct cpu(kernel/proc.h)

Xv6 maintains a `struct cpu` for each CPU (kernel/proc.h:22), which records the process currently running on that CPU (if any), `saved registers` for the CPU’s scheduler thread, and the count of nested `spinlocks` needed to manage interrupt disabling.

The function `mycpu` (kernel/proc.c:72) returns a pointer to the current CPU’s `struct cpu`.
RISC-V numbers its CPUs, giving each a `hartid`. Xv6 ensures that each CPU’s hartid is stored in that CPU’s `tp` register while in the kernel.
This allows mycpu to use `tp` to index an array of cpu structures to find the right one.
Ensuring that a CPU’s `tp` always holds the CPU’s hartid is a little involved.
`mstart` sets the `tp` register early in the CPU’s boot sequence, while still in **machine mode** (kernel/start.c:51).
`usertrapret` saves `tp` in the trampoline page, because the user process might modify tp. Finally,
`uservec` restores that saved `tp` when entering the kernel from user space (kernel/trampoline.S:70).

The compiler guarantees never to use the `tp` register. It would be more convenient if xv6 could ask the RISC-V hardware for the current hartid whenever needed, but RISC-V allows that only in **machine mode**, not in **supervisor mode**.

The return values of `cpuid` and `mycpu` are fragile: if the timer were to interrupt and cause the thread to yield and then move to a different CPU, a previously returned value would no longer be correct.
To avoid this problem, xv6 requires that callers **disable interrupts**, and only enable them after they finish using the returned struct cpu.
The function myproc (kernel/proc.c:80) returns the struct proc pointer for the process that is running on the current CPU. myproc disables interrupts, invokes mycpu, fetches the current process pointer (c->proc) out of the struct cpu, and then enables interrupts. The return value of myproc is safe to use even if interrupts are enabled: if a timer interrupt moves the calling process to a different CPU, its struct proc pointer will stay the same.

## spin.c

```
#include "kernel/types.h"
#include "user/user.h"


int main(int argc, char *argv[])
{
    int pid;
    char c;

    pid = fork();
    if(pid == 0){
        c = '/';
    }else{
        printf("parent pid=%d, child pid=%d\n",getpid(), pid);
        c='\\';
    }
    for(int i=0;;i++){
        if((i%100000)==0){
            write(2, &c, 1);
        }
    }
    exit(0);
}
```

## Debug

### usertrap -> devintr  (kernel/trap.c)

```bash
$ make clean && make CPUS=1 qemu-gdb
$ spin

# set a breakpoint at devintr
(gdb) b trap.c:207
Breakpoint 1 at 0x8000274e: file kernel/trap.c, line 207.
(gdb) c
Continuing.

Thread 1 hit Breakpoint 1, devintr () at kernel/trap.c:207
207         if(cpuid() == 0){
=> 0x000000008000274e <devintr+128>:    97 f0 ff ff     auipc   ra,0xfffff
   0x0000000080002752 <devintr+132>:    e7 80 a0 1f     jalr    506(ra) # 0x80001948 <cpuid>
   0x0000000080002756 <devintr+136>:    01 c9   beqz    a0,0x80002766 <devintr+152>
(gdb) where
#0  devintr () at kernel/trap.c:207
#1  0x000000008000280a in usertrap () at kernel/trap.c:68
#2  0x0000000000000060 in ?? ()

(gdb) finish
(gdb) where
#0  usertrap () at kernel/trap.c:76
#1  0x0000000000000060 in ?? ()

(gdb) p which_dev
$2 = 2

# struct proc *p = myproc();
# read and write tp, the thread pointer, which holds
#this core's hartid (core number), the index into cpus[].

(gdb) print p
$1 = (struct proc *) 0x80011978 <proc+720>

# what process in running on the core?
(gdb) print p->name
$2 =   "spin", '\000' <repeats 11 times>

(gdb) p p->pid
$3 = 3

(gdb) p/x *(p->trapframe)
$8 = {
  kernel_satp = 0x8000000000087fff,
  kernel_sp = 0x3fffffa000,
  kernel_trap = 0x80002770,
  epc = 0x60,    # user program counter
  kernel_hartid = 0x1,
  ra = 0x60,     # return address
  sp = 0x2fb0,   # stack pointer
  gp = 0x505050505050505,
  tp = 0x505050505050505,
  t0 = 0x505050505050505,
  t1 = 0x505050505050505,
  t2 = 0x505050505050505,
  s0 = 0x2fe0,
  s1 = 0x484a7ead,
  a0 = 0x1,
  a1 = 0x2fbf,
  a2 = 0x1,
  a3 = 0x2e91,
  a4 = 0x0,
  a5 = 0xa2ad,
  a6 = 0x0,
  a7 = 0x10,
  s2 = 0x186a0,
  s3 = 0x20,
  s4 = 0x1463,
  s5 = 0x13e8,
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


(gdb) p/x p->trapframe->epc
$9 = 0x60


# code user/spin.asm:
# 0x60 is the address of the addiw instruction

  56:	4509                	li	a0,2
  58:	00000097          	auipc	ra,0x0
  5c:	2a8080e7          	jalr	680(ra) # 300 <write>
    for(int i=0;;i++){
  60:	2485                	addiw	s1,s1,1
        if((i%100000)==0){
  62:	0324e7bb          	remw	a5,s1,s2
  66:	ffed                	bnez	a5,60 <main+0x60>
  68:	b7e5                	j	50 <main+0x50>
```

### yield (kernel/proc.c)

```c
// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}
```

```bash
(gdb) where
#0  yield () at kernel/proc.c:527
#1  0x0000000080002860 in usertrap () at kernel/trap.c:81
#2  0x0000000000000060 in ?? ()
(gdb) p p->pid
$6 = 3
```

### sched  (kernel/proc.c)

```c
// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct cpu *c = mycpu();
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = c->intena;
  // save current kernel thread registers in p
  // mycpu(): this cpu has the context, the same registers of this core' schduler thread
  // continue to run this core's scheduler thread
  swtch(&p->context, &c->context);
  mycpu()->intena = intena;
}
```

```bash
(gdb) where
#0  sched () at kernel/proc.c:508
#1  0x0000000080002172 in yield () at kernel/proc.c:529
#2  0x0000000080002860 in usertrap () at kernel/trap.c:81
#3  0x0000000000000060 in ?? ()
(gdb) p p->pid
$7 = 3

(gdb) p p->name
$8 =   "spin", '\000' <repeats 11 times>
(gdb) p/x p->context
$12 = {
  ra = 0x80001fe6,
  sp = 0x3fffff9f90,
  s0 = 0x3fffff9fc0,
  s1 = 0x0,
  s2 = 0x80011978,
  s3 = 0x80011290,
  s4 = 0x1463,
  s5 = 0x13e8,
  s6 = 0x505050505050505,
  s7 = 0x505050505050505,
  s8 = 0x505050505050505,
  s9 = 0x505050505050505,
  s10 = 0x505050505050505,
  s11 = 0x505050505050505
}

(gdb) p/x cpus[0].context
$14 = {
  ra = 0x80001f0e,
  sp = 0x8000a0f0,
  s0 = 0x8000a150,
  s1 = 0x80011ae0,
  s2 = 0x0,
  s3 = 0x4,
  s4 = 0x800170a8,
  s5 = 0x2,
  s6 = 0x80011290,
  s7 = 0x3,
  s8 = 0x800112b0,
  s9 = 0x0,
  s10 = 0x0,
  s11 = 0x0
}

(gdb) p c->proc
$17 = (struct proc *) 0x80011978 <proc+720>
(gdb) p c->proc->pid
$18 = 3
(gdb) p p
$19 = (struct proc *) 0x80011978 <proc+720>

# check ra register instructions: return address point to scheduler()
(gdb) x/i 0x80001f0e
   0x80001f0e <scheduler+106>:  sd      zero,24(s6)

```

### swtch (kernel/swtch.S)

```asm
# Context switch
#
#   void swtch(struct context *old, struct context *new);
#
# Save current registers in old. Load from new.


.globl swtch
swtch:
        sd ra, 0(a0)   # a0 the current kernel process thread context
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)  # a1 the cpu/core's scheduler thread  context
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)

        ret
```

```bash
(gdb) tbreak swtch
Temporary breakpoint 2 at 0x80002542
(gdb) c
Continuing.

Thread 3 hit Temporary breakpoint 2, 0x0000000080002542 in swtch ()
=> 0x0000000080002542 <swtch+0>:        23 30 15 00     sd      ra,0(a0)


(gdb) where
#0  0x0000000080002542 in swtch ()
#1  0x0000000080001fe6 in sched () at kernel/proc.c:518
#2  0x0000000080002172 in yield () at kernel/proc.c:529
#3  0x0000000080002860 in usertrap () at kernel/trap.c:81
#4  0x0000000000000060 in ?? ()

(gdb) p/x $a0
$5 = 0x80011b40
(gdb) p/x $a1
$6 = 0x800113b0

(gdb) info r $a0 $a1
a0             0x800119d8       2147555800
a1             0x800113b0       2147554224


# $ra is the return addres: we come here from sched function
(gdb) p $ra
$7 = (void (*)()) 0x80001fe6 <sched+126>

(gdb) p $sp
$8 = (void *) 0x3fffff7f90

(gdb) p $pc
$23 = (void (*)()) 0x80002542 <swtch>


# we just need save the Callee Saved Registers(14) when switching threads
(gdb) stepi 28

# stack pointer is pointing to the new thread's stack: stack0 ,
# stack0 is very very early booting sequence
(gdb) p $sp
$9 = (void *) 0x8000c0f0 <stack0+12160

(gdb) p $pc
$10 = (void (*)()) 0x800025aa <swtch+104>

(gdb) p $ra
# Now the ra is pointing to the scheduler, so when ret return , it will jump to scheduler
$11 = (void (*)()) 0x80001f0e <scheduler+106>

(gdb) where
#0  0x00000000800025aa in swtch ()
#1  0x0000000080001f0e in scheduler () at kernel/proc.c:479
#2  0x0000000080000f24 in main () at kernel/main.c:44

```

The function `swtch` performs the saves and restores for **a kernel thread switch**.

#### contexts

`swtch` doesn’t directly know about threads; it just saves and restores sets of 32 RISC-V registers, called `contexts`.
When it is time for a process to give up the CPU, the process’s kernel thread calls `swtch` to save its own context and return to the scheduler context.
Each context is contained in a `struct context` (kernel/proc.h:2), itself contained in a process’s `struct proc` or a CPU’s `struct cpu`.

Swtch takes two arguments:

- `struct context *old`
- `struct context *new`

It saves the current registers in old, loads registers from new, and returns.

Let’s follow a process through `swtch` into the scheduler. We saw in Chapter 4 that one possibility at the end of an interrupt is that `usertrap` calls `yield`. `Yield` in turn calls `sched`, which calls `swtch` to save the current context in `p->context` and `switch` to the scheduler context previously saved in `cpu->scheduler`(kernel/proc.c:490).

#### callee-saved registers

`Swtch` (kernel/swtch.S:3) saves only **callee-saved registers**;
the C compiler generates code in the caller to save **caller-saved registers** on the stack. Swtch knows the offset of each register’s field in struct context. It does not save `the program counter`. Instead, swtch saves the `ra` register, which holds the return address from which swtch was called.

Now swtch restores registers from the new context, which holds register values saved by a previous swtch. When swtch returns, it returns to the instructions pointed to by the restored `ra` register, that is, the instruction from which the new thread previously called `swtch`.
In addition, it returns on the new thread’s stack, since that’s where the restored `sp` points.

In our example, `sched` called swtch to switch to cpu->scheduler, the per-CPU scheduler context. That context was saved at the point in the past when scheduler called `swtch` (kernel/proc.c:456) to switch to the process that’s now giving up the CPU. When the swtch we have been tracing returns, it returns not to sched but to scheduler, with the stack pointer in the current CPU’s scheduler stack.


QA: 为什么只需要保存 callee-save 寄存器？

RISC-V 调用约定里寄存器分两类
1. Caller-save（调用者保存）
```
a0–a7   参数 / 返回值
t0–t6   临时寄存器
```
规则是： 函数调用后，这些寄存器值“可能被破坏”
所以：
- thread A 调用 thread_switch
- 它不能指望 a0/t0 这些还能保持原样
- 编译器早就默认你会丢
所以不用保存

1. Callee-save（被调用者保存）
```
s0–s11  保存寄存器
sp      栈指针
ra      返回地址
```

规则是： 
函数必须保证：返回时这些寄存器和调用前一样

但我们要干的事是： “假装从函数返回，但其实跑到另一个线程去了”

所以：
- 必须手动保存当前线程的这些寄存器
- 并从另一个线程里恢复它们

只保存 callee-save，刚刚好，不多不少


### scheduler  (kernel/proc.c)

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

    int nproc = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state != UNUSED) {
        nproc++;
      }
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        //  save the scheduler registers, restore the target process's registers
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
    if(nproc <= 2) {   // only init and sh exist
      intr_on();
      asm volatile("wfi");
    }
  }
}
```

```bash
(gdb) stepi
scheduler () at kernel/proc.c:483
=> 0x0000000080001f0e <scheduler+106>:  23 3c 0b 00     sd      zero,24(s6)

(gdb) p p->pid
$14 = 3

```

The last section looked at the low-level details of swtch; now let’s take swtch as a given and
examine switching from one process’s kernel thread through the scheduler to another process.

The scheduler exists in the form of **a special thread per CPU**, each running the scheduler function.
This function is in charge of choosing which process to run next.

A process that wants to give up the CPU must acquire its own process lock `p->lock`, release any other locks it is
holding, update its own state (p->state), and then call `sched`. You can see this sequence in
yield (kernel/proc.c:496), sleep and exit.

Sched double-checks some of those requirements (kernel/proc.c:480-485) and then checks an implication: since a lock is held, interrupts should be disabled.
Finally, `sched` calls `swtch` to save the current context in `p->context` and switch to `the scheduler context` in cpu->scheduler.
Swtch returns on the scheduler’s stack as though scheduler’s swtch had returned The scheduler continues its for loop, finds a process to run, switches to it, and the cycle repeats.

We just saw that xv6 holds `p->lock` across calls to swtch:
the caller of swtch must already hold the lock, and control of the lock passes to the switched-to code.
This convention is unusual with locks;
usually the thread that acquires a lock is also responsible for releasing the lock, which makes it easier to reason about correctness.
For context switching it is necessary to break this convention because `p->lock` protects invariants on the process’s state and context fields that are not true while executing in swtch.
One example of a problem that could arise if p->lock were not held during swtch: a different CPU might decide to run the process after yield had set its state to RUNNABLE, but before swtch caused it to stop using its own kernel stack.
The result would be two CPUs running on the same stack, which would cause chaos.

#### coroutines: sched & scheduler

The only place a kernel thread gives up its CPU is in `sched`, and it always switches to the same location in `scheduler`, which (almost) always switches to some kernel thread that previously called `sched`.
Thus, if one were to print out the line numbers where xv6 switches threads, one would observe the following simple pattern: (kernel/proc.c:456), (kernel/proc.c:490), (kernel/proc.c:456), (kernel/proc.c:490), and so on.

Procedures that intentionally transfer control to each other via thread switch are sometimes referred to as `coroutines`; in this example, `sched` and `scheduler` are **co-routines** of each other.
There is one case when the scheduler’s call to swtch does not end up in sched. `allocproc` sets the context ra register of a new process to `forkret` (kernel/proc.c:508), so that its **first swtch** “returns” to the start of that function. Forkret exists to release the p->lock; otherwise, since the new process needs to return to user space as if returning from fork, it could instead start at `usertrapret`.

Scheduler (kernel/proc.c:438) runs a loop:

- find a process to run
- run it until it yields
- repeat.

The scheduler loops over the process table looking for **a runnable process**, one that has `p->state== RUNNABLE`.
Once it finds a process, it sets the per-CPU current process variable c->proc, marks the process as `RUNNING`, and then calls swtch to start running it (kernel/proc.c:451-456).

One way to think about the structure of the scheduling code is that it enforces a set of invariants about each process, and holds p->lock whenever those invariants are not true.

One invariant is that if a process is RUNNING, a timer interrupt’s yield must be able to safely switch away from
the process; this means that the CPU registers must hold the process’s register values (i.e. swtch
hasn’t moved them to a context), and c->proc must refer to the process.

Another invariant is that if a process is `RUNNABLE`, it must be safe for an idle CPU’s scheduler to run it; this means
that p->context must hold the process’s registers (i.e., they are not actually in the real registers),
that no CPU is executing on the process’s kernel stack, and that no CPU’s c->proc refers to the process.

Observe that these properties are often not true while p->lock is held.
Maintaining the above invariants is the reason why xv6 often `acquires p->lock` in one thread and releases it in another, for example `acquiring in yield` and `releasing in scheduler`. Once yield has started to modify a running process’s state to make it RUNNABLE, the lock must remain held until the invariants are restored: the earliest correct release point is after scheduler (running on its own stack) clears c->proc.
Similarly, once scheduler starts to convert a RUNNABLE process to RUNNING, the lock cannot be released until the kernel thread is completely running (after the swtch, for example in yield).

#### &p->lock

holding the lock across switching to a process

1. when yielding the CPU involves multiple steps, this lock prevent any other core's schduler from looking at our process until all three steps below have completed, lock making those steps atomic
   yielding the CPU involves multiple steps:
   - change the state of the current yielding process from `RUNNING` to `RUNABLE`
   - save the registers in the yielding process context
   - stop using the yielding process stack
2. when start runing a process, the lock have a similar protetive function , making the starting a process to be atomic
   - change the state of the process start running from `RUNABLE` to `RUNNING`
   - move the registers from the process registers to the RISC-V registers

```bash
(gdb) p proc[2]
$19 = {
  lock = {
    locked = 1,
    name = 0x8000b1b0 "proc",
    cpu = 0x80014318 <cpus+128>
  },
  state = RUNNABLE,
  parent = 0x80014800 <proc+360>,
  chan = 0x0,
  killed = 0,
  xstate = 0,
  pid = 3,
  kstack = 274877878272,
  sz = 12288,
  pagetable = 0x87f49000,
  trapframe = 0x87f65000,
  context = {
    ra = 2147496578,
    sp = 274877882240,
    s0 = 274877882288,
    s1 = 2281066496,
    s2 = 361700864190383365,
    s3 = 361700864190383365,
    s4 = 361700864190383365,
    s5 = 361700864190383365,
    s6 = 361700864190383365,
    s7 = 361700864190383365,
    s8 = 361700864190383365,
    s9 = 361700864190383365,
    s10 = 361700864190383365,
    s11 = 361700864190383365
  },
  ofile =     {0x800243b0 <ftable+24>,
    0x800243b0 <ftable+24>,
    0x800243b0 <ftable+24>,
    0x0 <repeats 13 times>},
  cwd = 0x800227c0 <icache+24>,
  name =     "spin", '\000' <repeats 11 times>
}


(gdb) p/x proc[2]->context
$20 = {
  ra = 0x80003282,
  sp = 0x3fffff9f80,
  s0 = 0x3fffff9fb0,
  s1 = 0x87f65000,
  s2 = 0x505050505050505,
  s3 = 0x505050505050505,
  s4 = 0x505050505050505,
  s5 = 0x505050505050505,
  s6 = 0x505050505050505,
  s7 = 0x505050505050505,
  s8 = 0x505050505050505,
  s9 = 0x505050505050505,
  s10 = 0x505050505050505,
  s11 = 0x505050505050505
}


(gdb) p proc[3]
$21 = {
  lock = {
    locked = 0,
    name = 0x8000b1b0 "proc",
    cpu = 0x0
  },
  state = RUNNABLE,
  parent = 0x80014968 <proc+720>,
  chan = 0x0,
  killed = 0,
  xstate = 0,
  pid = 4,
  kstack = 274877870080,
  sz = 12288,
  pagetable = 0x87f76000,
  trapframe = 0x87f4a000,
  context = {
    ra = 2147496578,
    sp = 274877874048,
    s0 = 274877874096,
    s1 = 2280955904,
    s2 = 361700864190383365,
    s3 = 361700864190383365,
    s4 = 361700864190383365,
    s5 = 361700864190383365,
    s6 = 361700864190383365,
    s7 = 361700864190383365,
    s8 = 361700864190383365,
    s9 = 361700864190383365,
    s10 = 361700864190383365,
    s11 = 361700864190383365
  },
  ofile =     {0x800243b0 <ftable+24>,
    0x800243b0 <ftable+24>,
    0x800243b0 <ftable+24>,
    0x0 <repeats 13 times>},
  cwd = 0x800227c0 <icache+24>,
  name =     "spin", '\000' <repeats 11 times>
}


(gdb) next

# // Switch to chosen process.  It is the process's job
# // to release its lock and then reacquire it
# // before jumping back to us.
# p->state = RUNNING;
# c->proc = p;

# switch to process 4
(gdb) p p->pid
$22 = 4
(gdb) p p
$23 = (struct proc *) 0x80014ad0 <proc+1080>
(gdb) p p->state
$24 = RUNNABLE
(gdb) p c->proc
$25 = (struct proc *) 0x0

(gdb) p/x c->context
$28 = {
  ra = 0x80003170,
  sp = 0x8000dfd0,
  s0 = 0x8000e000,
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

#
(gdb) p/x p->context
$29 = {
  ra = 0x80003282,
  sp = 0x3fffff7f80,
  s0 = 0x3fffff7fb0,
  s1 = 0x87f4a000,
  s2 = 0x505050505050505,
  s3 = 0x505050505050505,
  s4 = 0x505050505050505,
  s5 = 0x505050505050505,
  s6 = 0x505050505050505,
  s7 = 0x505050505050505,
  s8 = 0x505050505050505,
  s9 = 0x505050505050505,
  s10 = 0x505050505050505,
  s11 = 0x505050505050505
}

(gdb) next


(gdb) where
#0  scheduler () at kernel/proc.c:479
#1  0x0000000080001918 in main () at kernel/main.c:44

(gdb) p p->state
$32 = RUNNING

(gdb) p c->proc
$33 = (struct proc *) 0x80014ad0 <proc+1080>

(gdb) p/x p->context
$38 = {
  ra = 0x80003282,
  sp = 0x3fffff7f80,
  s0 = 0x3fffff7fb0,
  s1 = 0x87f4a000,
  s2 = 0x505050505050505,
  s3 = 0x505050505050505,
  s4 = 0x505050505050505,
  s5 = 0x505050505050505,
  s6 = 0x505050505050505,
  s7 = 0x505050505050505,
  s8 = 0x505050505050505,
  s9 = 0x505050505050505,
  s10 = 0x505050505050505,
  s11 = 0x505050505050505
}

(gdb) p/x p->context->ra
$39 = 0x80003282

(gdb) x/i p->context->ra
   0x80003282 <sched+196>:      auipc   ra,0xfffff


(gdb) tb swtch
Temporary breakpoint 3 at 0x80003712
(gdb) c
Continuing.

Thread 2 hit Temporary breakpoint 3, 0x0000000080003712 in swtch ()
=> 0x0000000080003712 <swtch+0>:        23 30 15 00     sd      ra,0(a0)
```

### swtch()

this time switch from the scheduler to the new process

```bash
(gdb) where
#0  0x0000000080003712 in swtch ()
#1  0x0000000080003170 in scheduler () at kernel/proc.c:479
#2  0x0000000080001918 in main () at kernel/main.c:44

(gdb) stepi 28
0x000000008000377a in swtch ()
=> 0x000000008000377a <swtch+104>:      82 80   ret
(gdb) p $ra
$40 = (void (*)()) 0x80003282 <sched+196>

(gdb) where
#0  0x000000008000377a in swtch ()
#1  0x0000000080003282 in sched () at kernel/proc.c:518
#2  0x00000000800032ce in yield () at kernel/proc.c:529
#3  0x0000000080003a9c in usertrap () at kernel/trap.c:81
#4  0x0000000000000086 in ?? ()

(gdb) stepi
sched () at kernel/proc.c:519
=> 0x0000000080003282 <sched+196>:      97 f0 ff ff     auipc   ra,0xfffff
   0x0000000080003286 <sched+200>:      e7 80 a0 47     jalr    1146(ra) # 0x800026fc <mycpu>
   0x000000008000328a <sched+204>:      2a 87   mv      a4,a0

(gdb) where
#0  sched () at kernel/proc.c:519
#1  0x00000000800032ce in yield () at kernel/proc.c:529
#2  0x0000000080003a9c in usertrap () at kernel/trap.c:81
#3  0x0000000000000086 in ?? ()
```

### sched() -> yield() -> usertrap()

# Sleep and wakeup : coordination

## 1. 线程切换过程中锁的限制

首先是上节课的回顾。

### 1. acquire process lock before calling switch

在 XV6 中，任何时候调用 switch 函数都会从一个线程切换到另一个线程，通常是在用户进程的内核线程和调度器线程之间切换。
在调用 switch 函数之前，总是会先获取线程对应的用户进程的锁。所以过程是这样，一个进程先获取自己的锁，然后调用 switch 函数切换到调度器线程，调度器线程再释放进程锁。

```
                         |    scheduler
acquire(&p->lock);       |
p->state = RUNNABLE;     |
swtch();                 |    swtch()
                         |    release(&p->lock);
```

实际上的代码顺序更像这样：

1. 一个进程出于某种原因想要进入休眠状态，比如说出让 CPU 或者等待数据，它会先获取自己的锁 `&p->lock` ；
2. 之后进程将自己的状态从 `RUNNING` 设置为 `RUNNABLE` ；
3. 之后进程调用 `swtch` 函数，其实是调用 `sched` 函数在 `sched` 函数中再调用的 `swtch` 函数；
4. `swtch` 函数将当前的线程切换到调度器线程；
5. 调度器线程之前也调用了 `swtch` 函数，现在恢复执行会从自己的 `swtch` 函数返回；
6. 返回之后，调度器线程会释放刚刚出让了 CPU 的进程的锁

在第 1 步中获取进程的锁的原因是:
这样可以阻止其他 CPU 核的调度器线程在当前进程完成切换前，发现进程是 RUNNABLE 的状态并尝试运行它。

为什么要阻止呢？
因为其他每一个 CPU 核都有一个调度器线程在遍历进程表单，如果没有在进程切换的最开始就获取进程的锁的话，其他 CPU 核就有可能在当前进程还在运行时，认为该进程是 RUNNABLE 并运行它。而两个 CPU 核使用同一个栈运行同一个线程会使得系统立即崩溃。

所以，在进程切换的最开始，进程先获取自己的锁，并且直到调用 `swtch` 函数时也不释放锁。
而另一个线程，也就是调度器线程会在进程的线程完全停止使用自己的栈之后，再释放进程的锁。
释放锁之后，就可以由其他的 CPU 核再来运行进程的线程，因为这些线程现在已经不在运行了。

### 2. No other locks for swtch

XV6 中，在线程切换的过程中，不允许进程在执行 switch 函数的过程中，持有任何其他的锁。所以，进程在调用 switch 函数的过程中，必须要持有 p->lock（注，也就是进程对应的 proc 结构体中的锁），但是同时又不能持有任何其他的锁。

这也是包含了 Sleep 在内的很多设计的限制条件之一。如果你是一个 XV6 的程序员，你需要遵循这条规则。接下来让我解释一下背后的原因，首先构建一个不满足这个限制条件的场景：

我们有进程 P1，P1 的内核线程持有了 p->lock 以外的其他锁，这些锁可能是在使用磁盘，UART，console 过程中持有的。之后内核线程在持有锁的时候，通过调用 switch/yield/sched 函数出让 CPU，这会导致进程 P1 持有了锁，但是进程 P1 又不在运行。

```
No other locks for swtch

   P1             |          P2
   acquire(L)     |
   swtch(P2)      |
                  |        acquire(L) -> Deadlock
                  |
                  |
   [release[L]]   |

```

假设我们在一个只有一个 CPU 核的机器上，进程 P1 调用了 switch 函数将 CPU 控制转给了调度器线程，调度器线程发现还有一个进程 P2 的内核线程正在等待被运行，所以调度器线程会切换到运行进程 P2。
假设 P2 也想使用磁盘，UART 或者 console，它会对 P1 持有的锁调用 acquire，这是对于同一个锁的第二个 acquire 调用。
当然这个锁现在已经被 P1 持有了，所以这里的 acquire 并不能获取锁。
假设这里是 spinlock，那么进程 P2 会在一个循环里不停的“旋转”并等待锁被释放。但是很明显进程 P2 的 acquire 不会返回，所以即使进程 P2 稍后愿意出让 CPU，P2 也没机会这么做。之所以没机会是因为 P2 对于锁的 acquire 调用在直到锁释放之前都不会返回，而唯一锁能被释放的方式就是进程 P1 恢复执行并在稍后 release 锁，但是这一步又还没有发生，因为进程 P1 通过调用 switch 函数切换到了 P2，而 P2 又在不停的“旋转”并等待锁被释放。这是一种死锁，它会导致系统停止运行。

### QA

- 学生提问：难道定时器中断不会将 CPU 控制切换回进程 P1 从而解决死锁的问题吗？

Robert 教授：首先，所有的进程切换过程都发生在内核中，所有的 acquire，switch，release 都发生在内核代码而不是用户代码。实际上 XV6 允许在执行内核代码时触发中断，如果你查看 trap.c 中的代码你可以发现，如果 XV6 正在执行内核代码时发生了定时器中断，中断处理程序会调用 yield 函数并出让 CPU。

但是在之前的课程中我们讲过 acquire 函数在等待锁之前会 关闭中断**turn off interrupts**，否则的话可能会引起死锁（注，详见 10.8），所以我们不能在等待锁的时候处理中断。
所以如果你查看 XV6 中的 `acquire` 函数，你可以发现函数中第一件事情就是关闭中断 `push_off()`，之后再“自旋”等待锁释放。

你或许会想，为什么不能先“自旋”等待锁释放，再关闭中断？
因为这样会有一个短暂的时间段锁被持有了但是中断没有关闭，在这个时间段内的设备的中断处理程序可能会引起死锁。

so when we acquire a lock, we need **turn off interrupts** in `acquire()` function, otherwise, when interrupts are enabled and yeild, the scheduler will be called, and another process maybe have a same lock, then it is a Deadlock

```c
void
acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  lk->cpu = mycpu();
}
```

所以不幸的是，当我们在自旋等待锁释放时会关闭中断，进而阻止了定时器中断并且阻止了进程 P2 将 CPU 出让回给进程 P1。嗯，这是个好问题。

学生提问：能重复一下死锁是如何避免的吗？

Robert 教授：在 XV6 中，死锁是通过禁止在线程切换的时候加锁来避免的。
XV6 禁止在调用 switch 函数时，获取除了 `p->lock` 以外的其他锁。如果你查看 sched 函数的代码（注，详见 11.6），里面包含了一些检查代码来确保除了 p->lock 以外线程不持有其他锁。所以上面会产生死锁的代码在 XV6 中是不合法的并被禁止的。

```c
void
sched(void)
{
  int intena;
  struct cpu *c = mycpu();
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = c->intena;
  swtch(&p->context, &c->context);
  mycpu()->intena = intena;
}
```

## 2. Sleep & Wakeup 接口

**Scheduling** and **locks** help conceal the actions of one thread from another, but we also need abstractions that help **threads intentionally interact**.

For example

- the reader of a pipe in xv6 may need to wait for a writing process to produce data;
- a parent’s call to wait may need to wait for a child to exit;
- a process reading the disk needs to wait for the disk hardware to finish the read.
- The xv6 kernel uses a mechanism called **sleep and wakeup** in these situations (and many others).

`Sleep` allows a kernel thread to wait for a specific event; 
another thread can call `wakeup` to indicate that threads waiting for an event should resume.
Sleep and wakeup are often called **sequence coordination** or **conditional synchronization** mechanisms.

### Coordination Examples

接下来看一下通过 Sleep & Wakeup 实现 Coordination。

我们听过很多关于锁的介绍，
- 锁可以使得线程本身不必关心其他线程的具体实现。
- 我们为共享的数据增加锁，这样就不用担心其他线程也使用了相同的数据，因为锁可以确保对于数据的操作是依次发生的。

当你在写一个线程的代码时，有些场景需要等待一些特定的事件，或者不同的线程之间需要交互。

- 假设我们有一个 Pipe，并且我正在从 Pipe 中读数据。但是 Pipe 当前又没有数据，所以我需要等待一个 Pipe 非空的事件。

- 类似的，假设我在读取磁盘，我会告诉磁盘控制器请读取磁盘上的特定块。这或许要花费较长的时间，尤其当磁碟需要旋转时

（通常是毫秒级别），磁盘才能完成读取。而执行读磁盘的进程需要等待读磁盘结束的事件。

- 类似的，一个 Unix 进程可以调用 wait 函数。这个会使得调用进程等待任何一个子进程退出。所以这里父进程有意的在等待另一个进程产生的事件。

以上就是进程需要等待特定事件的一些例子。特定事件可能来自于 I/O，也可能来自于另一个进程，并且它描述了某件事情已经发生。Coordination 是帮助我们解决这些问题并帮助我们实现这些需求的工具。Coordination 是非常基础的工具，就像锁一样，在实现线程代码时它会一直出现。

我们怎么能让进程或者线程等待一些特定的事件呢？

### Busy-wait

一种非常直观的方法是通过循环实现 busy-wait。假设我们想从一个 Pipe 读取数据，我们就写一个循环一直等待 Pipe 的 buffer 不为空。

```c
while(pipe_empty(pipe)){
}
```

这个循环会一直运行直到其他的线程向 Pipe 的 buffer 写了数据。之后循环会结束，我们就可以从 Pipe 中读取数据并返回。

实际中会有这样的代码。如果你知道你要等待的事件极有可能在 0.1 微秒内发生，通过循环等待或许是最好的实现方式。
通常来说在操作设备硬件的代码中会采用这样的等待方式，如果你要求一个硬件完成一个任务，并且你知道硬件总是能非常快的完成任务，这时通过一个类似的循环等待或许是最正确的方式。

另一方面，事件可能需要数个毫秒甚至你都不知道事件要多久才能发生，或许要 10 分钟其他的进程才能向 Pipe 写入数据，那么我们就不想在这一直循环并且浪费本可以用来完成其他任务的 CPU 时间。这时我们想要通过类似 `switch` 函数调用的方式出让 CPU，并在我们关心的事件发生时重新获取 CPU。

Coordination 就是有关出让 CPU，直到等待的事件发生再恢复执行。人们发明了很多不同的 Coordination 的实现方式，但是与许多 Unix 风格操作系统一样，XV6 使用的是 `Sleep & Wakeup` 这种方式。


### Example: UART

介绍完背景了，接下来我们看一下 XV6 的代码。为了准备这节课，我重写了 UART 的驱动代码，XV6 通过这里的驱动代码从 console 中读写字符。

#### uartwrite

```c

static int tx_done; // has the UART finished sending?
static int tx_chan; // &tx_chan is the "wait channel"

void
uartwrite(char buf[], int n)
{
  acquire(&uart_tx_lock);
  int i=0;
  while(i<n){
    while(tx_done == 0){
      // UART is busy sending a character. wait for it to interrupt
      sleep(&tx_chan, &uart_tx_lock);
    }
    WriteReg(THR, buf[i]);
    i += 1;
    tx_done = 0;
  }
  release(&uart_tx_lock);
}
```

首先是 uartwrite 函数。
当 shell 需要输出时会调用 write 系统调用最终走到 uartwrite 函数中，这个函数会在循环中将 buf 中的字符一个一个的向 UART 硬件写入。
这是一种经典的设备驱动实现风格，你可以在很多设备驱动中看到类似的代码。

UART 硬件一次只能接受一个字符的传输，而通常来说会有很多字符需要写到 UART 硬件。
你可以向 UART 硬件写入一个字符，并等待 UART 硬件说：好的我完成了传输上一个字符并且准备好了传输下一个字符，之后驱动程序才可以写入下一个字符。
因为这里的硬件可能会非常慢，或许每秒只能传输 1000 个字符，所以我们在两个字符之间的等待时间可能会很长。而 1 毫秒在现在计算机上是一个非常非常长的时间，它可能包含了数百万条指令时间，所以我们不想通过循环来等待 UART 完成字符传输，我们想通过一个更好的方式来等待。如大多数操作系统一样，XV6 也的确存在更好的等待方式。



#### uartintr

UART 硬件会在完成传输一个字符后，触发一个中断。所以 UART 驱动中除了 uartwrite 函数外，还有名为 uartintr 的中断处理程序。这个中断处理程序会在 UART 硬件触发中断时由 trap.c 代码调用。

```c
// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from trap.c.
void
uartintr(void)
{
  // read and process incoming characters.
  acquire(&uart_tx_lock);
  if(ReadReg(LSR) & LSR_TX_IDLE){
    // UART finished transmitting; wake up any sending thread
    tx_done = 1;
    wakeup(&tx_chan);
  }
  release(&uart_tx_lock);
  // read and process incoming characters.
  while(1){
    int c = uartgetc();
    if(c==-1)
      break;
  }
}

```

中断处理程序会在最开始读取UART对应的memory mapped register，并检查其中表明传输完成的相应的标志位，也就是 `LSR_TX_IDLE` 标志位。如果这个标志位为1，代码会将 `tx_done` 设置为1，并调用 `wakeup` 函数。
这个函数会使得 `uartwrite` 中的 `sleep` 函数恢复执行，并尝试发送一个新的字符。

所以这里的机制是，如果一个线程需要等待某些事件，比如说等待 UART 硬件愿意接收一个新的字符，线程调用 `sleep` 函数并等待一个特定的条件。当特定的条件满足时，代码会调用 `wakeup` 函数。这里的 `sleep` 函数和 `wakeup` 函数是成对出现的。

我们之后会看sleep函数的具体实现，它会做很多事情最后再调用switch函数来出让CPU。

这里有件事情需要注意，sleep 和 wakeup 函数需要通过某种方式链接到一起。也就是说，如果我们调用wakeup函数，我们只想唤醒正在等待刚刚发生的特定事件的线程。所以，sleep函数和wakeup函数都带有一个叫做 `sleep channel` 的参数。
我们在调用wakeup的时候，需要传入与调用sleep函数相同的 `sleep channel`。不过sleep和wakeup函数只是接收表示了sleep channel的64bit数值，它们并不关心这个数值代表什么。当我们调用sleep函数时，我们通过一个 sleep channel 表明我们等待的特定事件，当调用wakeup时我们希望能传入相同的数值来表明想唤醒哪个线程。


#### QA
有关这里的接口有什么问题吗？

学生提问：进程会在写入每个字符时候都被唤醒一次吗？

Robert教授：在这个我出于演示目的而特别改过的UART驱动中，传输每个字符都会有一个中断，所以你是对的，对于buffer中的每个字符，我们都会等待UART可以接收下一个字符，之后写入一个字符，将tx_done设置为0，回到循环的最开始并再次调用sleep函数进行睡眠状态，直到tx_done为1。当UART传输完了这个字符，uartintr函数会将tx_done设置为1，并唤醒uartwrite所在的线程。所以对于每个字符都有调用一次sleep和wakeup，并占用一次循环。

UART实际上支持一次传输4或者16个字符，所以一个更有效的驱动会在每一次循环都传输16个字符给UART，并且中断也是每16个字符触发一次。更高速的设备，例如以太网卡通常会更多个字节触发一次中断。

以上就是接口的演示。Sleep&wakeup的一个优点是它们可以很灵活，它们不关心代码正在执行什么操作，你不用告诉sleep函数你在等待什么事件，你也不用告诉wakeup函数发生了什么事件，你只需要匹配好 64bit 的 **sleep channel** 就行。

不过，对于sleep函数，有一个有趣的参数，我们需要将一个锁作为第二个参数传入，这背后是一个大的故事，我后面会介绍背后的原因。总的来说，不太可能设计一个sleep函数并完全忽略需要等待的事件。所以很难写一个通用的sleep函数，只是睡眠并等待一些特定的事件，并且这也很危险，因为可能会导致lost wakeup，而几乎所有的Coordination机制都需要处理lost wakeup的问题。在sleep接口中，我们需要传入一个锁是一种稍微丑陋的实现，我在稍后会再介绍。



### Lost wakeup

在解释sleep函数为什么需要一个锁使用作为参数传入之前，我们先来看看假设我们有了一个更简单的不带锁作为参数的sleep函数，会有什么样的结果。这里的结果就是 lost wakeup。

#### broken_sleep

假设sleep只是接收任意的sleep channel作为唯一的参数。它其实不能正常工作，我们称这个sleep 实现为 `broken_sleep` 。
你可以想象一个sleep函数内会将进程的状态设置为SLEEPING，表明当前进程不想再运行，而是正在等待一个特定的事件。如果你们看过了XV6的实现，你们可以发现sleep函数中还会做很多其他操作。我们需要记录特定的sleep `channel值，这样之后的wakeup` 函数才能发现是当前进程正在等待 `wakeup` 对应的事件。最后再调用switch函数出让CPU。

```c
broken_sleep(int chan):
  p->state =  SLEEPING;
  p->chan = chan;
  swtch();

wakeup(chan)
for (p = proc; p < &proc[NPROC]; p++) {
  if (p->state == SLEEPING && p->chan == chan)
    p->state = RUNNABLE;
}

```
如果sleep函数只做了这些操作，那么很明显sleep函数会出问题，我们至少还应该在这里获取进程的锁。
之后是wakeup函数。我们希望唤醒所有正在等待特定 `sleep channel` 的线程。所以wakeup函数中会查询进程表单中的所有进程，如果进程的状态是 `SLEEPING` 并且进程对应的 channel 是当前 `wakeup` 的参数，那么将进程的状态设置为 `RUNNABLE` 。
```
Producer: process -> sleep(chan) -> SLEEPING  -> swtch()
Consumer:         -> wakeup(chan) -> RUNNABLE -> swtch()
```

在一些平行宇宙中，sleep & wakeup 或许就是这么简单。在我回到XV6代码之前，让我演示一下如何在UART驱动中使用刚刚介绍的sleep和wakeup函数。这基本上是重复前一节的内容，不过这次我们使用刚刚介绍的稍微简单的接口。


首先是定义done标志位。之后是定义uartwrite函数。在函数中，对于buffer内的每一个字符，检查done标志位，如果标志位为0，就调用sleep函数并传入tx_channel。之后将字符传递给UART并将done设置为0。

```c
int done;
uartwrite(buf){
  for each c in buf 
     while not done:
       broken_sleep(tx_chan);
     send c
     done=0
}

uatintr(){
  acquire(&uart_tx_lock);
  done=1
  wakeup(&tx_chan);
  realse(&uart_tx_lock);
}
```

以上就是使用broken_sleep的方式。这里缺失的是锁。这里 `uartwrite` 和 `uartintr` 两个函数需要使用锁来协调工作。

- 第一个原因是done标志位，任何时候我们有了共享的数据，我们需要为这个数据加上锁。
- 另一个原因是两个函数都需要访问UART硬件，通常来说让两个线程并发的访问 `memory mapped register` 是错误的行为。

所以我们需要在两个函数中加锁来避免对于done标志位和硬件的竞争访问。

现在的问题是，我们该在哪个位置加锁？在中断处理程序中较为简单，我们在最开始加锁，在最后解锁。

难的是如何在 `uartwrite` 函数中加锁。

1. 一种可能是，每次发送一个字符的过程中持有锁，所以在每一次遍历buffer的起始和结束位置加锁和解锁。

```c
int done;
uartwrite(buf){
  for each c in buf 
     lock
     while not done: 
       broken_sleep(tx_chan);
     send c
     done=0
     unlock
}

```
为什么这样肯定不能工作？
一个原因是，我们能从while not done的循环退出的唯一可能是中断处理程序将 done 设置为1。但是如果我们为整个代码段都加锁的话，中断处理程序就不能获取锁了，中断程序会不停“自旋”并等待锁释放。而锁被 `uartwrite` 持有，在done设置为1之前不会释放。而done只有在中断处理程序获取锁之后才可能设置为1。所以我们不能在发送每个字符的整个处理流程都加锁。

上面加锁方式的问题是， `uartwrite` 在期望中断处理程序执行的同时又持有了锁。而我们唯一期望中断处理程序执行的位置就是sleep函数执行期间，其他的时候 uartwrite 持有锁是没有问题的。

2. 所以另一种实现可能是，在传输字符的最开始获取锁
  因为我们需要保护共享变量done，但是在调用 sleep 函数之前释放锁。这样中断处理程序就有可能运行并且设置done标志位为1。之后在sleep函数返回时，再次获取锁。
  让我来修改代码，并看看相应的运行结果是什么。现有的代码中，uartwrite在最开始获取了锁，并在最后释放了锁。
  中断处理程序也在最开始获取锁，之后释放锁。

  接下来，我们会探索为什么只接收一个参数的broken_sleep在这不能工作。为了让锁能正常工作，我们需要在调用broken_sleep函数之前释放uart_tx_lock，并在 broken_sleep 返回时重新获取锁。broken_sleep 内的代码与之前在白板上演示的是一样的。也就是首先将进程状态设置为SLEEPING，并且保存tx_chan到进程结构体中，最后调用switch函数。

```c
int done;
uartwrite(buf){
  lock
  for each c in buf 
     while not done: 
       unlock
       broken_sleep(tx_chan);
       lock
     send c
     done=0
  unlock
}


void
uartwrite(char buf[], int n)
{ 
  acquire(&uart_tx_lock);
  int i=0;
  printf("uartwrite with broken_sleep");
  while(i<n){
    while(tx_done == 0){
      release(&uart_tx_lock);
      broken_sleep(&tx_chan);
      acquire(&uart_tx_lock);
    }
    WriteReg(THR, buf[i]);
    i += 1;
    tx_done = 0;
  }
  release(&uart_tx_lock);
}
```

在XV6启动的时候会打印“init starting”，这里看来输出了一些字符之后就hang住了。如果我输入任意字符，剩下的字符就能输出。这里发生了什么？

这里的问题必然与之前修改的代码相关。在前面的代码中，sleep之前释放了锁，但是在释放锁和broken_sleep之间可能会发生中断。

```c
void
uartwrite(char buf[], int n)
{ 
  acquire(&uart_tx_lock);
  int i=0;
  printf("uartwrite with broken_sleep");
  while(i<n){
    while(tx_done == 0){
      release(&uart_tx_lock);
      // RIGHT HERE --- INTERRUPT 
      broken_sleep(&tx_chan);
      acquire(&uart_tx_lock);
    }
    WriteReg(THR, buf[i]);
    i += 1;
    tx_done = 0;
  }
  release(&uart_tx_lock);
}
```
一旦释放了锁，当前CPU的中断会被重新打开。因为这是一个多核机器，所以中断可能发生在任意一个CPU核。
在上面代码标记的位置，其他CPU核上正在执行UART的中断处理程序，并且正在acquire函数中等待当前锁释放。所以一旦锁被释放了，另一个CPU核就会获取锁，并发现UART硬件完成了发送上一个字符，之后会设置tx_done为1，最后再调用 wakeup 函数，并传入tx_chan。
目前为止一切都还好，除了一点：现在写线程还在执行并位于 `release` 和 `broken_sleep` 之间，也就是写线程还没有进入 `SLEEPING` 状态，所以中断处理程序中的 `wakeup` 并没有唤醒任何进程，因为还没有任何进程在 `tx_chan` 上睡眠。
之后写线程会继续运行，调用 `broken_sleep` ，将进程状态设置为 `SLEEPING` ，保存 sleep channel。
但是中断已经发生了， `wakeup` 也已经被调用了。所以这次的broken_sleep，没有人会唤醒它，因为wakeup已经发生过了。
这就是 **lost wakeup** 问题。

#### QA
学生提问：是不是总是这样，一旦一个wakeup被丢失了，下一次wakeup时，之前缓存的数据会继续输出？

Robert教授：这完全取决于实现细节。
在我们的例子中，实际上出于偶然才会出现当我输入某些内容会导致之前的输出继续的现象。这里背后的原因是，我们的代码中，UART只有一个中断处理程序。不论是有输入，还是完成了一次输出，都会调用到同一个中断处理程序中。所以当我输入某些内容时，会触发输入中断，之后会调用uartintr函数。然后在中断处理程序中又会判断LSR_TX_IDLE标志位，并再次调用wakeup，所以刚刚的现象完全是偶然。如果出现了lost wakeup问题，并且你足够幸运的话，某些时候它们能自动修复。
如果UART有不同的接收和发送中断处理程序的话，那么就没办法从lost wakeup恢复。

学生提问：tx_done标志位的作用是什么？

Robert教授：这是一种简单的在uartintr和uartwrite函数之间通信的方法。tx_done标志位为1表示已经完成了对于前一个字符的传输，并且uartwrite可以传输下一个字符，所以这是用来在中断处理程序和uartwrite之间通信的标志位。

同一个学生提问：当从sleep函数中唤醒时，不是已经知道是来自UART的中断处理程序调用wakeup的结果吗？这样的话tx_done有些多余。

Robert教授：我想你的问题也可以描述为：为什么需要通过一个循环while(tx_done == 0)来调用sleep函数？这个问题的答案适用于一个更通用的场景：实际中不太可能将sleep和wakeup精确匹配。并不是说sleep函数返回了，你等待的事件就一定会发生。

举个例子，假设我们有两个进程同时想写UART，它们都在uartwrite函数中。可能发生这种场景，当一个进程写完一个字符之后，会进入SLEEPING状态并释放锁，而另一个进程可以在这时进入到循环并等待UART空闲下来。之后两个进程都进入到SLEEPING状态，当发生中断时UART可以再次接收一个字符，两个进程都会被唤醒，但是只有一个进程应该写入字符，所以我们才需要在sleep外面包一层while循环。实际上，你可以在XV6中的每一个sleep函数调用都被一个while循环包着。因为事实是，你或许被唤醒了，但是其他人将你等待的事件拿走了，所以你还得继续sleep。这种现象还挺普遍的。

学生提问：我们只看到了一个lost wakeup，当我们随便输入一个字符，整个剩下的字符都能输出，为什么没有在输出剩下字符的时候再次发生lost wakeup？

Robert教授：这会发生的。我来敲一下cat README，这会输出数千个字符。可以看到每过几个字符就会hang一次，需要我再次输入某个字符。这个过程我们可以看到很多lost wakeup。之前之所以没有出现，是因为lost wakeup需要中断已经在等待获取锁，并且uartwrite位于release和broken_sleep之间，这需要一定的巧合并不总是会发生。

### 如何避免Lost wakeup

现在我们的目标是消灭掉lost wakeup。这可以通过消除下面的窗口时间来实现。
```c
      release(&uart_tx_lock);
      // RIGHT HERE --- INTERRUPT 
      broken_sleep(&tx_chan);
      acquire(&uart_tx_lock);
```

首先我们必须要释放 `uart_tx_lock` 锁，因为中断需要获取这个锁，但是我们又不能在释放锁和进程将自己标记为 `SLEEPING` 之间留有窗口。这样中断处理程序中的 `wakeup` 才能看到 `SLEEPING` 状态的进程，并将其唤醒，进而我们才可以避免 lost wakeup的问题。所以，我们应该消除这里的窗口。

为了实现这个目的，我们需要将sleep函数设计的稍微复杂点。这里的解决方法是，即使sleep函数不需要知道你在等待什么事件，它还是需要你知道你在等待什么数据，并且传入一个用来保护你在等待数据的锁。sleep函数需要特定的条件才能执行，而sleep自己又不需要知道这个条件是什么。在我们的例子中，sleep函数执行的特定条件是 tx_done 等于1。

虽然sleep不需要知道tx_done，但是它需要知道保护这个条件的锁，也就是这里的 uart_tx_lock。在调用sleep的时候，锁还被当前线程持有，之后这个锁被传递给了sleep。

在接口层面，sleep承诺可以 **原子性** 的将 **进程设置成 SLEEPING 状态**，同时 **释放锁**。
这样 `wakeup` 就不可能看到这样的场景：锁被释放了但是进程还没有进入到SLEEPING状态。
所以sleep这里将 **释放锁** 和 **设置进程为SLEEPING状态** 这两个行为合并为一个 `原子操作` 。

所以我们需要有一个锁来保护sleep的条件，并且这个锁需要传递给sleep作为参数。


更进一步的是，当调用wakeup时，锁必须被持有。如果程序员想要写出正确的代码，都必须遵守这些规则来使用sleep和wakeup。

接下来我们看一下sleep和wakeup如何使用这一小块额外的信息（注，也就是传入给sleep函数的锁）和刚刚提到的规则，来避免lost wakeup。



### Code: Sleep and wakeup


Xv6’s sleep (kernel/proc.c:529) and wakeup (kernel/proc.c:560) provide the interface shown in the last example above, and their implementation (plus rules for how to use them) ensures that there are **no lost wakeups**. 

#### The basic idea 
The basic idea is to 
- have `sleep` mark the current process as `SLEEPING` and then call `sched` to release the CPU; 
- `wakeup` looks for a process sleeping on the given wait channel and marks it as `RUNNABLE`. 
- Callers of sleep and wakeup can use any mutually convenient number as the channel. 
  Xv6 often uses the address of a kernel data structure involved in the waiting.

#### wakeup

首先我们来看一下proc.c中的 wakeup 函数。

```c
// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}
```
wakeup函数并不十分出人意料。它查看整个进程表单，对于每个进程首先加锁，这点很重要。
之后查看进程的状态，如果进程当前是SLEEPING并且进程的channel与wakeup传入的channel相同，将进程的状态设置为RUNNABLE。
最后再释放进程的锁。


#### uartwrite

```c

static int tx_done; // has the UART finished sending?
static int tx_chan; // &tx_chan is the "wait channel"

void
uartwrite(char buf[], int n)
{
  acquire(&uart_tx_lock);
  int i=0;
  while(i<n){
    while(tx_done == 0){
      // UART is busy sending a character. wait for it to interrupt
      sleep(&tx_chan, &uart_tx_lock);
    }
    WriteReg(THR, buf[i]);
    i += 1;
    tx_done = 0;
  }
  release(&uart_tx_lock);
}
```
uartwrite在最开始获取了sleep的condition lock，并且一直持有condition lock直到调用sleep函数。所以它首先获取了condition lock，之后检查condition（注，也就是tx_done等于0），之后在持有condition lock的前提下调用了sleep函数。此时wakeup不能做任何事情，wakeup现在甚至都不能被调用直到调用者能持有condition lock。所以现在wakeup必然还没有执行。

sleep函数在释放condition lock之前，先获取了进程的锁。在释放了condition lock之后，wakeup就可以被调用了，但是除非wakeup获取了进程的锁，否则wakeup不能查看进程的状态。所以，在sleep函数中释放了condition lock之后，wakeup也还没有执行。

在持有进程锁的时候，将进程的状态设置为SLEEPING并记录sleep channel，之后再调用sched函数，这个函数中会再调用switch函数（注，详见11.6），此时sleep函数中仍然持有了进程的锁，wakeup仍然不能做任何事情。

如果你还记得的话，当我们从当前线程切换走时，调度器线程中会释放前一个进程的锁（注，详见11.8）。所以在调度器线程释放进程锁之后，wakeup才能终于获取进程的锁，发现它正在SLEEPING状态，并唤醒它。

这里的效果是由之前定义的一些规则确保的，这些规则包括了：
- 调用sleep时需要持有condition lock，这样sleep函数才能知道相应的锁。
- sleep函数只有在获取到进程的锁p->lock之后，才能释放condition lock。
- wakeup需要同时持有两个锁才能查看进程。

这样的话，我们就不会再丢失任何一个wakeup，也就是说我们修复了lost wakeup的问题。


#### sleep

```c
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be guaranteed that we won't miss any wakeup 
  // (wakeup locks p->lock,because wakeup also acquires p->lock before inspecting state.),
  // so it's okay to release lk.

  if(lk != &p->lock){  //DOC: sleeplock0
    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);
    // we need to release lk to allow interrupt routine be able to aquire it
    // but we warried about after we realse the lock, the interrupt routine will call wakeup, and might wakeup the process,  but we have not mark the process as SLEEPING,and then we lost the wakeup,
    // so we can not afford to calling wakeup after the lock released at this point,
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();
  // releases p->lock
  // switches to the scheduler
  // when we wake up, sched() reacquires p->lock before returning to sleep()

  // after waking up
  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &p->lock){
    release(&p->lock);
    acquire(lk);
  }
}


```

- Sleep acquires `p->lock` (kernel/proc.c:540). Now the process going to sleep holds both `p->lock` and `lk`. 
- Holding `lk` was necessary in the caller (in the example, P): it ensured that no other process (in the example, one running V) could start a call to `wakeup(chan)`. 
- Now that sleep holds `p->lock`, it is safe to release `lk`: some other process may start a call to `wakeup(chan)`, but `wakeup` will wait to acquire `p->lock`, and thus will wait until `sleep` has finished putting the process to `SLEEPING`, keeping the `wakeup` from missing the sleep.
- Now that sleep holds `p->lock` and no others, it can put the process to sleep by recording the sleep channel, changing the process state to SLEEPING, and calling sched (kernel/proc.c:544-547).
  
In a moment it will be clear why it’s critical that p->lock is not released (by scheduler) until after the process is marked `SLEEPING`.
At some point, a process will acquire the condition lock, set the condition that the sleeper is waiting for, and call wakeup(chan). It’s important that wakeup is called while holding the condition lock1. Wakeup loops over the process table (kernel/proc.c:560). It acquires the p->lock of each process it inspects, both because it may manipulate that process’s state and because p->lock ensures that sleep and wakeup do not miss each other. When wakeup finds a process in state SLEEPING with a matching chan, it changes that process’s state to RUNNABLE. 
The next time the scheduler runs, it will see that the process is ready to be run.


Why do the locking rules for sleep and wakeup ensure a sleeping process won’t miss a wakeup? 

The sleeping process holds either the condition lock or its own p->lock or both from a point before it checks the condition to a point after it is marked SLEEPING. The process calling wakeup holds both of those locks in wakeup’s loop. Thus the waker either makes the condition
true before the consuming thread checks the condition; or the waker’s wakeup examines the sleeping thread strictly after it has been marked SLEEPING. Then wakeup will see the sleeping process and wake it up (unless something else wakes it up first).

It is sometimes the case that multiple processes are sleeping on the same channel; for example, more than one process reading from a pipe. A single call to wakeup will wake them all up. One of them will run first and acquire the lock that sleep was called with, and (in the case of pipes)
read whatever data is waiting in the pipe. The other processes will find that, despite being woken up, there is no data to be read. From their point of view the wakeup was “spurious,” and they must sleep again. For this reason sleep is always called inside a loop that checks the condition.

No harm is done if two uses of sleep/wakeup accidentally choose the same channel: they will see spurious wakeups, but looping as described above will tolerate this problem. Much of the charm of sleep/wakeup is that it is both lightweight (no need to create special data structures to act as sleep channels) and provides a layer of indirection (callers need not know which specific process they are interacting with).



### Semaphore

Sleep and wakeup provide a relatively low-level synchronization interface.
To motivate the way they work in xv6, we’ll use them to build a higher-level synchronization mechanism called a `semaphore` [4] that coordinates producers and consumers (xv6 does not use semaphores). A semaphore maintains a count and provides two operations.

- The “V” operation (for the producer) increments the count.
- The “P” operation (for the consumer) waits until the count is non-zero,and then decrements it and returns.
  If there were only one producer thread and one consumer thread, and they executed on different CPUs, and the compiler didn’t optimize too aggressively, this implementation would be correct:

```c
100 struct semaphore {
101   struct spinlock lock;
102   int count;
103 };
104
105 void
106 V(struct semaphore *s)
107 {
108   acquire(&s->lock);
109   s->count += 1;
110   release(&s->lock);
111 }
112
113 void
114 P(struct semaphore *s)
115 {
116   while(s->count == 0)
117   ;
118   acquire(&s->lock);
119   s->count -= 1;
120   release(&s->lock);
121 }
```

The implementation above is expensive. If the producer acts rarely, the consumer will spend most of its time spinning in the while loop hoping for a non-zero count. The consumer’s CPU could probably find more productive work than `busy waiting` by repeatedly `polling s->count`. 
Avoiding busy waiting requires a way for the consumer to yield the CPU and resume only after V increments the count.


Here’s a step in that direction, though as we will see it is not enough. Let’s imagine a pair of calls, sleep and wakeup, that work as follows. 
- `Sleep(chan)` sleeps on the arbitrary value chan, called **the wait channel**. `Sleep` puts the calling process to sleep, releasing the CPU for other work. 
- `Wakeup(chan)` wakes all processes sleeping on chan (if any), causing their `sleep` calls to return. If no processes are waiting on chan, `wakeup` does nothing. 
  
  We can change the semaphore implementation to use sleep and wakeup (changes highlighted in yellow):

```c
200 void
201 V(struct semaphore *s)
202 {
203   acquire(&s->lock);
204   s->count += 1;
205   wakeup(s);
206   release(&s->lock);
207 }
208
209 void
210 P(struct semaphore *s)
211 {
212   while(s->count == 0)
213     sleep(s);
214   acquire(&s->lock);
215   s->count -= 1;
216   release(&s->lock);
217 }
```

#### The lost wake-up problem

P now gives up the CPU instead of spinning, which is nice. 
However, it turns out not to be straightforward to design `sleep` and `wakeup` with this interface without suffering from what is known as **the lost wake-up problem**. 
- Suppose that P finds that s->count == 0 on line 212. 
- While P is between lines 212 and 213, V runs on another CPU: it changes `s->count` to be nonzero and calls `wakeup`, which finds no processes sleeping and thus does nothing. 
- Now P continues executing at line 213: it calls `sleep` and goes to sleep. 

This causes a problem: P is asleep waiting for a V call that has already happened. 

Unless we get lucky and the producer calls V again, the consumer will wait forever even though the count is non-zero.
The root of this problem is that the invariant that **P only sleeps when s->count == 0** is violated by V running at just the wrong moment. 
An incorrect way to protect the invariant would be to move the lock acquisition (highlighted in yellow below) in P so that its check of the count and its call to sleep are atomic:

```c
300 void
301 V(struct semaphore *s)
302 {
303   acquire(&s->lock);
304   s->count += 1;
305   wakeup(s);
306   release(&s->lock);
307 }
308
309 void
310 P(struct semaphore *s)
311 {
312   acquire(&s->lock);
313   while(s->count == 0)
314     sleep(s);
315   s->count -= 1;
316   release(&s->lock);
317 }
```
One might hope that this version of P would avoid the lost wakeup because the lock prevents V from executing between lines 313 and 314. It does that, but it also `deadlocks`: P holds the lock while it sleeps, so V will block forever waiting for the lock.

We’ll fix the preceding scheme by changing sleep’s interface: 
- the `caller` must pass the `condition lock` to sleep so it can release the lock after the calling process is marked as asleep and waiting on the sleep channel. 
- The lock will force a `concurrent V` to wait until `P` has finished putting itself to sleep, 
- so that the `wakeup` will find the sleeping consumer and wake it up. 
- Once the consumer is awake again sleep reacquires the lock before returning.
- 
Our new correct sleep/wakeup scheme is usable as follows (change highlighted in yellow):

```c
400 void
401 V(struct semaphore *s)
402 {
403   acquire(&s->lock);
404   s->count += 1;
405   wakeup(s);
406   release(&s->lock);
407 }

408
409 void
410 P(struct semaphore *s)
411 {
412   acquire(&s->lock);
413   while(s->count == 0)
414     sleep(s, &s->lock);
415   s->count -= 1;
416   release(&s->lock);
417 }

```

The fact that P holds `s->lock` prevents V from trying to wake it up between P’s check of c->count and its call to sleep. Note, however, that we need sleep to atomically release s->lock and put the consuming process to sleep, in order to avoid lost wakeups.





## Pipe中的sleep和wakeup
前面我们介绍了在UART的驱动中，如何使用 sleep 和 wakeup 才能避免lost wakeup。
前面这个特定的场景中，sleep等待的condition是发生了中断并且硬件准备好了传输下一个字符。
在一些其他场景，内核代码会调用sleep函数并等待其他的线程完成某些事情。这些场景从概念上来说与我们介绍之前的场景没有什么区别，但是感觉上还是有些差异。

例如，在读写pipe的代码中，如果你查看pipe.c中的piperead函数，
```c
int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(pr->killed){
      release(&pi->lock);
      return -1;
    }
    sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread++ % PIPESIZE];
    if(copyout(pr->pagetable, addr + i, &ch, 1) == -1)
      break;
  }
  wakeup(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}

```


# Code: Wait, exit, and kill


接下来，我想讨论一下XV6面临的一个与Sleep&Wakeup相关的挑战，也就是如何关闭一个进程。每个进程最终都需要退出，我们需要清除进程的状态，释放栈。在XV6中，一个进程如果退出的话，我们需要释放用户内存，释放page table，释放trapframe对象，将进程在进程表单中标为REUSABLE，这些都是典型的清理步骤。当进程退出或者被杀掉时，有许多东西都需要被释放。

这里会产生的两大问题：

首先我们不能直接单方面的摧毁另一个线程，因为：另一个线程可能正在另一个CPU核上运行，并使用着自己的栈；也可能另一个线程正在内核中持有了锁；也可能另一个线程正在更新一个复杂的内核数据，如果我们直接就把线程杀掉了，我们可能在线程完成更新复杂的内核数据过程中就把线程杀掉了。我们不能让这里的任何一件事情发生。

另一个问题是，即使一个线程调用了exit系统调用，并且是自己决定要退出。它仍然持有了运行代码所需要的一些资源，例如它的栈，以及它在进程表单中的位置。当它还在执行代码，它就不能释放正在使用的资源。所以我们需要一种方法让线程能释放最后几个对于运行代码来说关键的资源。

记住这两个问题。

XV6有两个函数与关闭线程进程相关。第一个是exit，第二个是kill。让我们先来看位于proc.c中的exit函数。

## exit 系统调用
这就是exit系统调用的内容。从exit接口的整体来看，在最后它会
- 释放进程的内存和page table，
- 关闭已经打开的文件，
- 同时我们也知道父进程会从wait系统调用中唤醒，所以exit最终会导致父进程被唤醒。
  这些都是我们预期可以从exit代码中看到的内容。

从上面的代码中，首先exit函数关闭了所有已打开的文件。这里可能会很复杂，因为关闭文件系统中的文件涉及到引用计数，虽然我们还没学到但是这里需要大量的工作。不管怎样，一个进程调用exit系统调用时，会关闭所有自己拥有的文件。

接下来是类似的处理，进程有一个对于当前目录的记录，这个记录会随着你执行cd指令而改变。
在exit过程中也需要将对这个目录的引用释放给文件系统。

如果一个进程要退出，但是它又有自己的子进程，接下来需要设置这些子进程的父进程为init进程。

我们接下来会看到，每一个正在exit的进程，都有一个父进程中的对应的wait系统调用。父进程中的wait系统调用会完成进程退出最后的几个步骤。所以如果父进程退出了，那么子进程就不再有父进程，当它们要退出时就没有对应的父进程的wait。所以在exit函数中，会为即将exit进程的子进程重新指定父进程为init进程，也就是PID为1的进程。

之后，我们需要通过调用wakeup函数唤醒当前进程的父进程，当前进程的父进程或许正在等待当前进程退出。

接下来，进程的状态被设置为ZOMBIE。现在进程还没有完全释放它的资源，所以它还不能被重用。所谓的进程重用是指，我们期望在最后，进程的所有状态都可以被一些其他无关的fork系统调用复用，但是目前我们还没有到那一步。

现在我们还没有结束，因为我们还没有释放进程资源。我们在还没有完全释放所有资源的时候，通过调用sched函数进入到调度器线程。

到目前位置，进程的状态是ZOMBIE，并且进程不会再运行，因为调度器只会运行RUNNABLE进程。同时进程资源也并没有完全释放，如果释放了进程的状态应该是UNUSED。但是可以肯定的是进程不会再运行了，因为它的状态是ZOMBIE。所以调度器线程会决定运行其他的进程。

```c
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  // we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);
  
  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);

  acquire(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup1(original_parent);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&original_parent->lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

```

## wait 系统调用

通过Unix的exit和wait系统调用的说明，我们可以知道如果一个进程exit了，并且它的父进程调用了wait系统调用，父进程的wait会返回。
wait函数的返回表明当前进程的一个子进程退出了。所以接下来我们看一下wait系统调用的实现。

它里面包含了一个大的循环。当一个进程调用了wait系统调用，它会扫描进程表单，找到父进程是自己且状态是ZOMBIE的进程。从上一节可以知道，这些进程已经在exit函数中几乎要执行完了。之后由父进程调用的freeproc函数，来完成释放进程资源的最后几个步骤。



```c
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  // hold p->lock for the whole time to avoid lost
  // wakeups from a child's exit().
  acquire(&p->lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      if(np->parent == p){
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acquire(&np->lock);
        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&p->lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&p->lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &p->lock);  //DOC: wait-sleep
  }
}


```


我们看一下freeproc的实现，

这是关闭一个进程的最后一些步骤。如果由正在退出的进程自己在exit函数中执行这些步骤，将会非常奇怪。这里释放了trapframe，释放了page table。如果我们需要释放进程内核栈，那么也应该在这里释放。但是因为内核栈的guard page，我们没有必要再释放一次内核栈。不管怎样，当进程还在exit函数中运行时，任何这些资源在exit函数中释放都会很难受，所以这些资源都是由父进程释放的。

wait不仅是为了父进程方便的知道子进程退出，wait实际上也是进程退出的一个重要组成部分。在Unix中，对于每一个退出的进程，都需要有一个对应的wait系统调用，这就是为什么当一个进程退出时，它的子进程需要变成init进程的子进程。init进程的工作就是在一个循环中不停调用wait，因为每个进程都需要对应一个wait，这样它的父进程才能调用freeproc函数，并清理进程的资源。

当父进程完成了清理进程的所有资源，子进程的状态会被设置成UNUSED。之后，fork系统调用才能重用进程在进程表单的位置。

学生提问：在exit系统调用中，为什么需要在重新设置父进程之前，先获取当前进程的父进程？

Robert教授：这里其实就是在防止一个进程和它的父进程同时退出。通常情况下，一个进程exit，它的父进程正在wait，一切都正常。但是也可能一个进程和它的父进程同时exit。所以当子进程尝试唤醒父进程，并告诉它自己退出了时，父进程也在退出。这些代码我一年前还记得是干嘛的，现在已经记不太清了。它应该是处理这种父进程和子进程同时退出的情况。如果不是这种情况的话，一切都会非常直观，子进程会在后面通过wakeup函数唤醒父进程。

学生提问：为什么我们在唤醒父进程之后才将进程的状态设置为ZOMBIE？难道我们不应该在之前就设置吗？

Robert教授：正在退出的进程会先获取自己进程的锁，同时，因为父进程的wait系统调用中也需要获取子进程的锁，所以父进程并不能查看正在执行exit函数的进程的状态。这意味着，正在退出的进程获取自己的锁到它调用sched进入到调度器线程之间（注，因为调度器线程会释放进程的锁），父进程并不能看到这之间代码引起的中间状态。所以这之间的代码顺序并不重要。大部分时候，如果没有持有锁，exit中任何代码顺序都不能工作。因为有了锁，代码的顺序就不再重要，因为父进程也看不到进程状态。

这里我想要强调的是，直到子进程exit的最后，它都没有释放所有的资源，因为它还在运行的过程中，所以不能释放这些资源。相应的其他的进程，也就是父进程，释放了运行子进程代码所需要的资源。这样的设计可以让我们极大的精简exit的实现。