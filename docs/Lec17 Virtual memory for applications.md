今天的话题是用户应用程序使用的虚拟内存，它主要是受这篇1991年的[论文](https://pdos.csail.mit.edu/6.828/2020/readings/appel-li.pdf)的启发。

# 论文中的核心观点
今天论文中的核心观点是，用户应用程序也应该从灵活的虚拟内存中获得收益，也就是说用户应用程序也可以使用虚拟内存。
用户应用程序本身就是运行在虚拟内存之上，我们这里说的虚拟内存是指：User Mode或者应用程序想要使用与内核相同的机制，来产生Page Fault并响应Page Fault（注，详见Lec08，内核中几乎所有的虚拟内存技巧都基于Page Fault）。
也就是说User Mode需要能够修改PTE的Protection位（注，Protection位是PTE中表明对当前Page的保护，对应了4.3中的Writeable和Readable位）或者Privileged level。

今天的论文，通过查看6-7种不同的应用程序，来说明用户应用程序使用虚拟内存的必要性。这些应用程序包括了：
- Garbage Collector
- Data Compression Application
- Shared Virtual Memory

## 虚拟内存的一些特性


你可以发现这都是一些非常不同的应用程序，并且它们都依赖虚拟内存的一些特性来正常工作。所以第一个问题是，上面的应用程序需要的特性是什么？所以我们先来讨论一下需要的特性是什么？

### trap
   首先，你需要trap来使得发生在内核中的Page Fault可以传播到用户空间，然后在用户空间的handler可以处理相应的Page Fault，之后再以正常的方式返回到内核并恢复指令的执行。这个特性是必须的，否则的话，你不能基于Page Fault做任何事情。

###  Prot1 & ProtN & Unprot
第二个特性是Prot1，它会降低了一个内存Page的accessability。
   accessability的意思是指内存Page的读写权限。内存Page的accessability有不同的降低方式，例如，将一个可以读写的Page变成只读的，或者将一个只读的Page变成完全没有权限。

除了对于每个内存Page的Prot1，还有管理多个Page的ProtN。ProtN基本上等效于调用N次Prot1，

那为什么还需要有ProtN？
因为单次ProtN的损耗比Prot1大不了多少，使用ProtN可以将成本分摊到N个Page，使得操作单个Page的性能损耗更少。在使用Prot1时，你需要修改PTE的bit位，并且在Prot1的结束时，需要清除TLB（注，详见4.4 Translation Lookaside Buffer），而清除TLB比较费时。如果能对所有需要修改的内存Page集中清理一次TLB，就可以将成本分摊。所以ProtN等效于修改PTE的bit位N次，再加上清除一次TLB。如果执行了N次Prot1，那就是N次修改PTE的bit位，再加上清除N次TLB，所以ProtN可以减少清除TLB的次数，进而提升性能。

`Unprot` ： 它增加了内存Page的accessability，例如将本来只读的Page变成可读可写的。

### Dirty
除此之外，还需要能够查看内存Page是否是Dirty。

### map2
   map2使得一个应用程序可以将一个特定的内存地址空间映射两次，并且这两次映射拥有不同的accessability（注，也就是一段物理内存对应两份虚拟内存，并且两份虚拟内存有不同的accessability）。


XV6在用户程序中支持以上任意的特性吗？除了有类似于trap及其相关的alarm hander之外，XV6不支持任何一个以上的特性。XV6只有一个最小化的Unix接口，并不支持以上任何虚拟内存特性。尽管在XV6的内核中包含了所有的可用的虚拟内存的机制，但是并没有以系统调用的形式将它们暴露给用户空间。

论文的观点是，任何一个好的操作系统都应该以系统调用的形式提供以上特性，以供应用程序使用。

所以自然的，这就引出了另一个问题，当今的Unix系统的功能范围是什么？以上特性属于Unix的范畴吗？如果你查看现在的Unix系统，例如Linux，你会发现，或许并不与论文中描述的完全一样，但是这些特性都存在。在论文那个年代（1991年），某些操作系统只包含了部分以上特性，但是如今这些特性都已经在现代的Unix系统中广泛支持了。接下来我们看一下如何实现这些特性。

# mmap
mmap: 支持应用程序使用虚拟内存的系统调用
第一个或许也是最重要的一个，是一个叫做mmap的系统调用。它接收某个对象，并将其映射到调用者的地址空间中。

举个例子，如果你想映射一个文件，那么你需要将文件描述符传递给mmap系统调用。mmap系统调用有许多令人眼花缭乱的参数（注，mmap的具体说明可以参考man page）：

```bash
MMAP(2)                                                                                                  Linux Programmer's Manual                                                                                                 MMAP(2)

NAME
       mmap, munmap - map or unmap files or devices into memory

SYNOPSIS
       #include <sys/mman.h>

       void *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
       int munmap(void *addr, size_t length);

       See NOTES for information on feature test macro requirements.

DESCRIPTION
       mmap()  creates  a new mapping in the virtual address space of the calling process.  The starting address for the new mapping is specified in addr.  The length argument specifies the length of the mapping (which must be greater
       than 0).

       If addr is NULL, then the kernel chooses the (page-aligned) address at which to create the mapping; this is the most portable method of creating a new mapping.  If addr is not NULL, then the kernel takes  it  as  a  hint  about
       where to place the mapping; on Linux, the kernel will pick a nearby page boundary (but always above or equal to the value specified by /proc/sys/vm/mmap_min_addr) and attempt to create the mapping there.  If another mapping al‐
       ready exists there, the kernel picks a new address that may or may not depend on the hint.  The address of the new mapping is returned as the result of the call.

       The contents of a file mapping (as opposed to an anonymous mapping; see MAP_ANONYMOUS below), are initialized using length bytes starting at offset offset in the file (or other object) referred to by  the  file  descriptor  fd.
       offset must be a multiple of the page size as returned by sysconf(_SC_PAGE_SIZE).

       After the mmap() call has returned, the file descriptor, fd, can be closed immediately without invalidating the mapping.

       The prot argument describes the desired memory protection of the mapping (and must not conflict with the open mode of the file).  It is either PROT_NONE or the bitwise OR of one or more of the following flags:

       PROT_EXEC  Pages may be executed.

       PROT_READ  Pages may be read.

       PROT_WRITE Pages may be written.

       PROT_NONE  Pages may not be accessed.
```
第一个参数是一个你想映射到的特定地址，如果传入null表示不指定特定地址，这样的话内核会选择一个地址来完成映射，并从系统调用返回。

第二个参数是想要映射的地址段长度len。

第三个参数是Protection bit，例如读写R|W。

第四个参数我们会跳过不做讨论，它的值可以是MAP_PRIVATE。它指定了如果你更新了传入的对象，会发生什么。（注，第四个参数是flags，MAP_PRIVATE是其中一个值，在mmap文件的场景下，MAP_PRIVATE表明更新文件不会写入磁盘，只会更新在内存中的拷贝，详见man page）。

第五个参数是传入的对象，在上面的例子中就是文件描述符。

第六个参数是offset。

实际上，你们将会在下个lab实现基于文件的mmap，下个lab结合了XV6的文件系统和虚拟内存，进而实现mmap。


## mmap 作用
通过上面的系统调用，可以将文件描述符指向的文件内容，从起始位置加上offset的地方开始，映射到特定的内存地址（如果指定了的话），并且连续映射len长度。

### Memory Mapped File
这使得你可以实现Memory Mapped File，你可以将文件的内容带到内存地址空间，进而只需要方便的通过普通的指针操作，而不用调用read/write系统调用，就可以从磁盘读写文件内容。这是一个方便的接口，可以用来操纵存储在文件中的数据结构。

### Anonymous Memory
mmap还可以用作他途。除了可以映射文件之外，还可以用来映射匿名的内存（Anonymous Memory）。这是sbrk（注，详见8.2）的替代方案，你可以向内核申请物理内存，然后映射到特定的虚拟内存地址。

mmap是实现应用程序虚拟内存的核心系统调用之一，我们稍后会将它与之前提到的特性关联起来。

除此之外，还需要有一些系统调用来支持论文中讨论到的特性。

### mprotect
mprotect系统调用（注，详见man page）。当你将某个对象映射到了虚拟内存地址空间，你可以修改对应虚拟内存的权限，这样你就可以以特定的权限保护对象的一部分，或者整个对象。

```bash
MPROTECT(2)                                                                                              Linux Programmer's Manual                                                                                             MPROTECT(2)

NAME
       mprotect, pkey_mprotect - set protection on a region of memory

SYNOPSIS
       #include <sys/mman.h>

       int mprotect(void *addr, size_t len, int prot);

       #define _GNU_SOURCE             /* See feature_test_macros(7) */
       #include <sys/mman.h>

       int pkey_mprotect(void *addr, size_t len, int prot, int pkey);

DESCRIPTION
       mprotect() changes the access protections for the calling process's memory pages containing any part of the address range in the interval [addr, addr+len-1].  addr must be aligned to a page boundary.

       If the calling process tries to access memory in a manner that violates the protections, then the kernel generates a SIGSEGV signal for the process.

       prot is a combination of the following access flags: PROT_NONE or a bitwise-or of the other values in the following list:

       PROT_NONE  The memory cannot be accessed at all.

       PROT_READ  The memory can be read.

       PROT_WRITE The memory can be modified.

       PROT_EXEC  The memory can be executed.

       PROT_SEM (since Linux 2.5.7)
                  The memory can be used for atomic operations.  This flag was introduced as part of the futex(2) implementation (in order to guarantee the ability to perform atomic operations required by commands such as FUTEX_WAIT),
                  but is not currently used in on any architecture.

       PROT_SAO (since Linux 2.6.26)
                  The memory should have strong access ordering.  This feature is specific to the PowerPC architecture (version 2.06 of the architecture specification adds the SAO CPU feature, and it is available on POWER 7 or PowerPC
                  A2, for example).

       Additionally (since Linux 2.6.0), prot can have one of the following flags set:

       PROT_GROWSUP
                  Apply the protection mode up to the end of a mapping that grows upwards.  (Such mappings are created for the stack area on architectures—for example, HP-PARISC—that have an upwardly growing stack.)

       PROT_GROWSDOWN
```

举个例子，通过上图对于mprotect的调用将权限设置成只读（R），这时，对于addr到addr+len这段地址，load指令还能执行，但是store指令将会变成Page Fault。类似的，如果你想要将一段地址空间变成完成不可访问的，那么可以在mprotect中的权限参数传入None，那么任何对于addr到addr+len这段地址的访问，都会生成Page Fault。

### munmap
对应mmap还有一个系统调用 munmap，它使得你可以移除一个地址或者一段内存地址的映射关系。如果你好奇这里是怎么工作的，你应该查看这些系统调用的man page。

### sigaction
最后一个系统调用是 `sigaction` ，它本质上是用来处理signal。它使得应用程序可以设置好一旦特定的signal发生了，就调用特定的函数。可以给它传入函数f作为特定 signal的handler。
在Page Fault的场景下，生成的signal是 `segfault` 。你或许之前在用户代码中看过了segfault，通常来说当发生segfault时，应用程序会停止运行并crash。但是如果应用程序为segfault signal设置了handler，发生segfault时，应用程序不会停止，相应的handler会被内核调用，然后应用程序可以在handler中响应segfault。

当内核发现Page Fault时，或许会通过修复Page Table来使得应用程序还能继续执行。与内核响应Page Fault的方式类似，在这里的handler中或许会调用mprotect来修改内存的权限来避免segfault，这样应用程序的指令就可以恢复运行。

### sigalarm
与sigaction类似的有 sigalarm（译注：sigalarm 是traps lab 中实现的一个系统调用，不属于标准Unix 接口），在sigalarm中可以设置每隔一段时间就调用handler。sigaction也可以实现这个功能，只是它更加的通用，因为它可以响应不同类型的signal。

学生提问：看起来mprotect暗示了你可以为单独的地址添加不同的权限，然而在XV6中，我们只能为整个Page设置相同的权限，这里是有区别吗？

Frans教授：不，这里并没有区别，它们都在Page粒度工作。如果你好奇的话，有一个单独的系统调用可以查看Page的大小。


如果你回想前面提到过的虚拟内存特性，我们可以将它们对应到这一节描述的Unix接口中来。
- trap对应的是sigaction系统调用
- Prot1，ProtN和Unprot可以使用mprotect系统调用来实现。
  mprotect足够的灵活，你可以用它来修改一个Page的权限，也可以用它来修改多个Page的权限。当修改多个Page的权限时，可以获得只清除一次TLB的好处。
- 查看Page的Dirty位要稍微复杂点，并没有一个直接的系统调用实现这个特性，不过你可以使用一些技巧完成它，我稍后会介绍它。
- map2也没有一个系统调用能直接对应它，通过多次调用mmap，你可以实现map2特性。

或许并不完全受这篇论文所驱动，但是内核开发人员已经在操作系统中为现在的应用程序提供了这些特性。接下来，我将在框架层面简单介绍一下这些特性是如何实现的，之后再看看应用程序是如何使用这些特性。



# VM implementation for applications

第一个是虚拟内存系统为了支持这里的特性，具体会发生什么？这里我们只会讨论最重要的部分，并且它也与即将开始的mmap lab有一点相关，因为在mmap lab中你们将要做类似的事情。

## Virtual Memory Areas（VMAs）

在现代的Unix系统中，地址空间是由硬件 Page Table 来体现的，在Page Table中包含了地址翻译。但是通常来说，地址空间还包含了一些操作系统的数据结构，这些数据结构与任何硬件设计都无关，它们被称为**Virtual Memory Areas（VMAs）**。
- VMA会记录一些有关连续虚拟内存地址段的信息。在一个地址空间中，可能包含了多个section，每一个section都由一个连续的地址段构成，对于每个section，都有一个VMA对象。
- 连续地址段中的所有Page都有相同的权限，
- 并且都对应同一个对象VMA（例如一个进程的代码是一个section，数据是另一个section，它们对应不同的VMA，VMA还可以表示属于进程的映射关系，例如下面提到的Memory Mapped File）。

举个例子，如果进程有一个Memory Mapped File，那么对于这段地址，会有一个VMA与之对应，VMA中会包含文件的权限，以及文件本身的信息，例如文件描述符，文件的offset等。

在接下来的mmap lab中，你们将会实现一个非常简单版本的VMA，并用它来实现针对文件的mmap系统调用。你可以在VMA中记录mmap系统调用参数中的文件描述符和offset。

## User level trap
第二个部分我们了解的就不多了，它或许值得仔细看一下，也就是User level trap是如何实现的？
我们假设一个PTE被标记成invalid或者只读，而你想要向它写入数据。这时，CPU会跳转到kernel中的固定程序地址，也就是XV6中的trampoline代码（注，详见6.2）。kernel会保存应用程序的状态，在XV6中是保存到trapframe。之后再向虚拟内存系统查询，

现在该做什么呢？
虚拟内存系统或许会做点什么，例如在lazy lab和copy-on-write lab中，trap handler会查看Page Table数据结构。而在我们的例子中会查看VMA，并查看需要做什么。举个例子，如果是segfault，并且应用程序设置了一个handler来处理它，那么
- segfault事件会被传播到用户空间
- 并且通过一个到用户空间的upcall在用户空间运行handler
- 在handler中或许会调用mprotect来修改PTE的权限
- 之后handler返回到内核代码
- 最后，内核再恢复之前被中断的进程。

当内核恢复了中断的进程时，如果handler修复了用户程序的地址空间，那么程序指令可以继续正确的运行，如果哪里出错了，那么会通过trap再次回到内核，因为硬件还是不能翻译特定的虚拟内存地址。



学生提问：当我们允许用户针对Page Fault来运行handler代码时，这不会引入安全漏洞吗？

Frans教授：这是个很好的问题。会有安全问题吗？你们怎么想的？这会破坏User/kernel或者不同进程之间的隔离性吗？或者从另一个角度来说，你的问题是sigalarm会破坏隔离性吗？

当我们执行upcall的时候，upcall会走到设置了handler的用户空间进程中，所以handler与设置了它的应用程序运行在相同的context，相同的Page Table中。所以handler唯一能做的事情就是影响那个应用程序，并不能影响其他的应用程序，因为它不能访问其他应用程序的Page Table，或者切换到其他应用程序的Page Table。所以这里还好。

当然，如果handler没有返回，或者做了一些坏事，最终内核还是会杀掉进程。所以唯一可能出错的地方就是进程伤害了自己，但是它不能伤害任何其他进程。


# Applications
接下来，我将通过介绍几个例子来看一下如何使用之前介绍的内容。我会从一个非常简单的例子开始，之后我们会看一下Garbage Collector，因为很多同学都问了Garbage Collector的问题，所以GC是一个可以深入探讨的好话题。

## 构建大的缓存表
首先我想讨论的是一个非常简单的应用，它甚至都没有在论文中提到，但它却是展示这节课的内容非常酷的一个方法。这个应用里是构建一个大的缓存表，什么是缓存表？它是用来记录一些运算结果的表单。举个例子，你可以这么想，下面是我们的表单，它从0开始到n。表单记录的是一些费时的函数运算的结果，函数的参数就是0到n之间的数字。

如果这个表单在最开始的时候就预计算好了，那么当你想知道f(i)的结果是什么时，你需要做的就是查看表单的i槽位，并获取f(i)的值。这样你可以将一个费时的函数运算转变成快速的表单查找，所以这里一个酷的技巧就是预先将费时的运算结果保存下来。如果相同的计算需要运行很多很多次，那么预计算或许是一个聪明的方案。

这里的挑战是，表单可能会很大，或许会大过物理内存，这里可以使用论文提到的虚拟内存特性来解决这个挑战。
1. 首先，你需要分配一个大的虚拟地址段，但是并不分配任何物理内存到这个虚拟地址段。这里只是从地址空间获取了很大一段地址，并说我将要使用地址空间的这部分来保存表单。

但是现在表单中并没有内容，表单只是一段内存地址。如果你现在查找表单的i槽位，会导致Page Fault。所以这里的计划是，在发生Page Fault时，先针对对应的虚拟内存地址分配物理内存Page，之后计算f(i)，并将结果存储于tb[i]，也就是表单的第i个槽位，最后再恢复程序的运行。

这种方式的优势是，如果你需要再次计算f(i)，你不需要在进行任何费时的计算，只需要进行表单查找。即使接下来你要查找表单的i+1槽位，因为一个内存Page可能可以包含多个表单项，这时也不用通过Page Fault来分配物理内存Page。


不过如果你一直这么做的话，因为表单足够大，你最终还是会消耗掉所有的物理内存。所以Page Fault Handler需要在消耗完所有的内存时，回收一些已经使用过的物理内存Page。当然，你需要修改已经被回收了的物理内存对应的PTE的权限，这样在将来使用对应地址段时，就可以获得Page Fault。所以你需要使用Prot1或者ProtN来减少这些Page的accessbility。

为了更具体的描述这里的应用，我这里有个小的实现，我们可以看一看这里是如何使用现有的Unix特性


在main函数中，首先调用setup_sqrt_region函数，它会从地址空间分配地址段，但是又不实际分配物理Page。之后调用test_sqrt_region。


在test_sqrt_region中，会以随机数来遍历表单，并通过实际运算对应的平方根值，来检查表单中相应位置值是不是保存了正确的平方根值。在test_sqrt_region运行的过程中，会产生Page Fault，因为现在还并没有分配任何物理内存Page。

应用程序该如何收到Page Fault呢？


在setup_sqrt_region函数中有一段代码，通过将handle_sigsegv函数注册到了SIGSEGV事件。这样当segfault或者Page Fault发生时，内核会调用handle_sigsegv函数。



handle_sigsegv函数与你们之前看过很多很多次的trap代码非常相似。

它首先会获取触发Page Fault的地址，

之后调用mmap对这个虚拟内存地址分配一个物理内存Page（注，这里是mmap映射匿名内存）。这里的虚拟内存地址就是我们想要在表单中用来保存数据的地址。

然后我们为这个Page中所有的表单项都计算对应的平方根值，之后就完事了。

这个应用程序有点极端，它在运行的时候只会使用一个物理内存Page，所以不论上一次使用的Page是什么，在handle_sigsegv的最后都会通过munmap释放它。所以我们有一个巨大的表单，但是它只对应一个物理内存Page。

接下来我将运行一下这个应用程序。test_sqrt_region会随机查看表单的内容，所以可以假设这会触发很多Page Fault，但是可以看出表单中的所有内容都能通过检查。

所以，尽管这里有一个巨大的表单用来保存平方根，但是实际在物理内存中只有一个内存Page。这是一个简单的例子，它展示了用户应用程序使用之前提到的虚拟内存特性之后可以做的一些酷的事情。


## QA
学生提问：能再讲一下为什么一个物理内存Page就可以工作吗？我觉得这像是lazy allocation，但是区别又是什么呢？

Frans教授：当我们刚刚完成设置时，我们一个内存Page都没有，setup_sqrt_region分配了一个地址段，但是又立即通过munmap将与这个地址段关联的内存释放了。所以在启动的最开始对于表单并没有一个物理内存Page与之关联。

之后，当我们得到了一个Page Fault，这意味着整个表单对应的地址中至少有一个Page没有被映射，虽然实际上我们一个Page都没有映射。现在我们得到了一个Page Fault，我们只需要映射一个Page，在这个Page中，我们会存入i，i+1。。。的平方根（注，因为一个Page4096字节，一个double8个字节，所以一个Page可以保存512个表单项）。

因为这是第一个Page Fault，之前并没有映射了内存Page，所以不需要做任何事情。

之后，程序继续运行并且查找了表单中的更多项，如果查找一个没有位于已分配Page上的表单项时，会得到另一个Page Fault。这时，在handle_sigsegv会分配第二个内存Page，并为这个Page计算平方根的值。之后会munmap记录在last_page_base中的内存。

当然，在实际中我们永远也不会这么做，在实际中至少会保留一些内存Page，这里只是以一种极端的方式展示，你可以只通过内存中的一个Page来表示一个巨大的表单。所以在handle_sigsegv中，会释放上一次映射的内存Page。

之后程序继续运行，所以在任何一个时间，只有一个物理内存Page被使用了。很明显，你们在实际中不会这么做，这里更多的是展示前面提到特性的能力。



## Baker's Real-Time Copying Garbage Collector
