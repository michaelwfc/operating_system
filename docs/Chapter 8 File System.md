
# File System Implementation Overview

## API Example
```c
fd =  open("x/y", flags);

// write系统调用并没有使用offset作为参数，所以写入到文件的哪个位置是隐式包含在文件系统中，文件系统在某个位置必然保存了文件的offset。因为如果你再调用write系统调用，新写入的数据会从第4个字节开始。
write(fd, "abc", 3);
write(fd, "def", 3);

// XV6和所有的Unix文件系统都支持通过系统调用创建链接，给同一个文件指定多个名字。你可以通过调用link系统调用，为之前创建的文件“x/y”创建另一个名字“x/z”。
// 所以文件系统内部需要以某种方式跟踪指向同一个文件的多个文件名。
link("x/y", "x/z")

// 我们还可能会在文件打开时，删除或者更新文件的命名空间。例如，用户可以通过unlink系统调用来删除特定的文件名。如果此时相应的文件描述符还是打开的状态，那我们还可以向文件写数据，并且这也能正常工作。
unlink("x/z")

```

所以，在文件系统内部，文件描述符必然与某个对象关联，而这个对象不依赖文件名。这样，即使文件名变化了，文件描述符仍然能够指向或者引用相同的文件对象。所以，实际上操作系统内部需要对于文件有内部的表现形式，并且这种表现形式与文件名无关。

### 还存在其他的方式能组织存储系统
文件系统的目的是实现上面描述的API，也即是典型的文件系统API。但是，这并不是唯一构建一个存储系统的方式。如果只是在磁盘上存储数据，你可以想出一个完全不同的API。

举个例子，数据库也能持久化的存储数据，但是数据库就提供了一个与文件系统完全不一样的API。我们这节课关注在文件系统，文件系统通常由操作系统提供，而数据库如果没有直接访问磁盘的权限的话，通常是在文件系统之上实现的（注，早期数据库通常直接基于磁盘构建自己的文件系统，因为早期操作系统自带的文件系统在性能上较差，且写入不是同步的，进而导致数据库的ACID不能保证。不过现代操作系统自带的文件系统已经足够好，所以现代的数据库大部分构建在操作系统自带的文件系统之上）。


# File System Structure

## File System Structure Overview 

The xv6 file system implementation is organized in seven layers, shown in Figure 8.1. 
![image](../images/Figure%208.1-Layers%20of%20the%20xv6%20file%20system.png)

## Storage Device

SSD通常是0.1到1毫秒的访问时间，而HDD通常是在10毫秒量级完成读写一个disk block。

sectors和blocks。

- sector通常是磁盘驱动可以读写的最小单元，它过去通常是512字节。

- block通常是操作系统或者文件系统视角的数据。它由文件系统定义，在XV6中它是 1024字节。所以XV6中一个block对应两个sector。通常来说一个block对应了一个或者多个sector。

有的时候，人们也将磁盘上的sector称为block。所以这里的术语也不是很精确。


## Disk layout

| Region     | Block numbers       |
| ---------- | ------------------- |
| Boot       | 0                   |
| Superblock | 1                   |
| Log        | 2 – 31 (30 blocks)  |
| Inodes     | 32 – 44 (13 blocks) |
| Bitmap     | 45 – 46 (2 blocks)  |
| Data       | 47+                 |

从文件系统的角度来看磁盘还是很直观的。因为对于磁盘就是读写block或者sector，我们可以将磁盘看作是一个巨大的block的数组，数组从0开始，一直增长到磁盘的最后。

而文件系统的工作就是将所有的数据结构以一种能够在重启之后重新构建文件系统的方式，存放在磁盘上。虽然有不同的方式，但是XV6使用了一种非常简单，但是还挺常见的布局结构。

通常来说：
- block0:  要么没有用，要么被用作boot sector来启动操作系统。

- block1: 通常被称为 **super block**，它描述了文件系统。它可能包含磁盘上有多少个block共同构成了文件系统这样的信息。我们之后会看到XV6在里面会存更多的信息，你可以通过block1构造出大部分的文件系统信息。

- log:   在XV6中，block2-31 共 30个block。实际上log的大小可能不同。

- inode: block32-44 共 13 个block 。inode 区域（inode blocks）。 
  我之前说过多个inode会打包存在一个block中。
  一个 inode = 64 字节
  一个 block = 1024 字节
 所以： 一个 block 能装 1024 / 64 = 16 个 inode

 inode blocks: block 32 ~ block 45 = 45 - 32 + 1 = 14 个 block
 14 × 16 = 224 个 inode
 这个文件系统一共支持 224 个 inode

- bitmap block: 45-46( bitmap blocks  可能是1个或者2个)
  之后是 bitmap block，这是我们构建文件系统的默认方法，它只占据一个block。它记录了数据block是否空闲。
  
- data block: 47 + 
  之后就全是数据 block 了，数据block存储了文件的内容和目录的内容。

通常来说，bitmap block，inode blocks 和 log blocks被统称为 **metadata block**。 它们虽然不存储实际的数据，但是它们存储了能帮助文件系统完成工作的元数据。




# Inode layer 

provides individual files, each represented as an inode with a unique i-number and some blocks holding the file’s data. 

最重要的可能就是inode，这是代表一个文件的对象，并且它不依赖于文件名。
- inode number
实际上，inode是通过自身的编号来进行区分的，这里的编号就是个整数。所以文件系统内部通过一个数字，而不是通过文件路径名引用inode。
- nlink: link count
  The nlink field counts the number of directory entries that refer to this inode,in order to recognize when the on-disk inode and its data blocks should be freed. 
一个文件（inode）只能在link count为0的时候被删除。实际的过程可能会更加复杂，实际中还有一个openfd count，也就是当前打开了文件的文件描述符计数。一个文件只能在这两个计数器都为0的时候才能被删除。


## inode Structure

- `dinode` is how an inode lives on disk.
  Stored on disk inside inode blocks
  Fixed layout, packed tightly
  Contains only filesystem metadata
  No locks, no reference counts, no runtime state

- `inode` is how the same inode lives in memory while the kernel is using it.
Same file. Two incarnations. One persistent, one alive.

```c
// kernel/fs.h
// On-disk inode structure
struct dinode {
  short type;           // File type 
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // The number of directory entries that refer to a file, xv6 won’t free an inode
if its link count is greater than zero.
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses,The addrs array records the block numbers of the disk blocks holding the file’s content.
};

// size_of(struct dinode) = 64
// 在 xv6（RISC-V）里：
// short = 2 bytes
// uint = 4 bytes
// NDIRECT = 12
// NDIRECT + 1 = 13

// 逐项相加： 
// 前四个 short：4 × 2 = 8
// uint size：4
// addrs[13]：13 × 4 = 52
// 总计： 8 + 4 + 52 = 64 bytes
// kernel/file.h
// in-memory copy of an inode


struct inode {
  uint dev;           // Device number : (dev, inum) → dinode
  uint inum;          // Inode number
  int ref;            // Reference count : the number of C pointers referring to the in-memory inode
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?
  // They start out invalid, and when you call:  ilock(ip);

  short type;         // copy of disk inode: These are a cached copy of the dinode.
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};
```

dinode 是一个64字节的数据结构。
- type : 通常来说它有一个type字段，表明inode是文件,目录, 设备文件。
  A type of zero indicates that an ondisk inode is free.
- nlink字段，也就是link计数器，用来跟踪究竟有多少文件名指向了当前的inode。
- size字段，表明了文件数据有多少个字节。
  
- direct block number
  磁盘被切成一个个 block（在 xv6 里是 1024 字节）。
  inode 里保存的是：“这些 block 在磁盘上的编号”。

不同文件系统中的表达方式可能不一样，不过在XV6中接下来是一些block的编号，例如编号0，编号1，等等。

XV6的inode中总共有12个block编号。
inode中block number 0到block number 11都是direct block number
这12个block编号指向了构成文件的前12个block。
举个例子，如果文件只有2个字节，那么只会有一个block编号0，它包含的数字是磁盘上文件前2个字节的block的位置。

这意味着： 小文件（≤ 12 KB）只需要 inode 就能找到全部数据,  不用额外磁盘访问，快得很

- indirect block number
  inode 的第 13 个指针：不再指向数据， 而是指向 一个“索引 block”
它对应了磁盘上一个block，这个block包含了256个block number，这256个block number包含了文件的数据。

block number 12保存的indirect block number指向了另一个block。



### Lifecycle of an inode
```
disk:   dinode exists
        ↓
iget() → inode allocated in memory (ref++)
        ↓
ilock() → dinode read into inode
        ↓
use inode
        ↓
iput() → ref--
        ↓
ref==0 → inode cache entry reused later
```

At no point do you modify dinode directly.
You modify inode, then write it back.



### Q:inode 编号是怎么对应到磁盘 block 的？
假设inode是64字节，如果你想要读取inode10，那么你应该按照下面的公式去对应的block读取inode。

```c
32 + (inode_number *64)/1024
```
- inode 编号从 0 或 1 开始（xv6 从 1 开始用）
- 每个 inode 64 字节， 
- 乘出来是“在 inode 区域里的字节偏移”
- 除以 1024 → 落在哪个 block
- 再加上 inode 区域的起始 block（32）

Ex: inode 10 在哪？
```
10 * 64 = 640 bytes
640 / 1024 = 0
在 inode 区域的第 0 个 block, 也就是 block 32
```

### Q: XV6中文件最大的长度是多少呢？

max file size = (256 + 12) * 1024 bytes = 25KB

可以算出这里就是268KB，这么点大小能存个什么呢？所以这是个很小的文件长度，实际的文件系统，文件最大的长度会大的多得多。那可以做一些什么来让文件系统支持大得多的文件呢？

是的，可以用类似page table的方式，构建一个双重indirect block number指向一个block，这个block中再包含了256个indirect block number，每一个又指向了包含256个block number的block。这样的话，最大的文件长度会大得多（注，是256*256*1K）。这里修改了inode的数据结构，你可以使用类似page table的树状结构，也可以按照B树或者其他更复杂的树结构实现。XV6这里极其简单，基本是按照最早的Uinx实现方式来的，不过你可以实现更复杂的结构。

实际上，在接下来的File system lab中，你将会实现双重indirect block number来支持更大的文件。在下一个File system lab，你们需要将inode中的一个block number变成双重indirect block number，这个双重indirect block number将会指向一个包含了256个indirect block number的block，其中的每一个indirect block number再指向一个包含了256个block number的block，这样文件就可以大得多。


真实系统里还会有：
- double indirect
- triple indirect
用来支持 TB 级文件


### 学生提问：为什么每个block存储256个block编号？

Frans教授：因为每个编号是4个字节。1024/4 = 256。这又带出了一个问题，如果block编号只是4个字节(32 位)，磁盘最大能有多大？
是的，2的32次方（注，4TB）。有些磁盘比这个数字要大，所以通常人们会使用比32bit更长的数字来表示block编号。

4 字节 = 32 位 , 能表示的 block 数量： 2^32 ≈ 4.29 billion blocks
如果每个 block 是 1KB： 最大磁盘 ≈ 4TB

磁盘再大： 32 位就不够了, 现代文件系统会用 64 位 block number


### Q: 读取文件的第8000个字节
我们想要实现read系统调用。假设我们需要读取文件的第8000个字节，那么你该读取哪个block呢？从inode的数据结构中该如何计算呢？

对于8000，我们首先除以1024，也就是block的大小，得到大概是7。这意味着第7个block就包含了第8000个字节。所以直接在inode的direct block number中，就包含了第8000个字节的block。为了找到这个字节在第7个block的哪个位置，我们需要用8000对1024求余数，我猜结果是是832。所以为了读取文件的第8000个字节，文件系统查看inode，先用8000除以1024得到block number，然后再用8000对1024求余读取block中对应的字节。

总结一下，inode中的信息完全足够用来实现read/write系统调用，至少可以找到哪个disk block需要用来执行read/write系统调用。


# File system工作示例

## 启动XV6 FILESYSTEM with mkfs
首先我会启动XV6，这里有件事情我想指出。启动XV6的过程中，调用了makefs指令，来创建一个文件系统。
```bash
make qemu

mkfs/mkfs fs.img README  user/_cat user/_echo user/_forktest user/_grep user/_init user/_kill user/_ln user/_ls user/_mkdir user/_rm user/_sh user/_stressfs user/_usertests user/_grind user/_wc user/_zombie  user/_bigfile
nmeta 70 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 25) blocks 199930 total 200000

balloc: first 537 blocks have been allocated
balloc: write bitmap block at sector 45
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 1 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

init: starting sh

```

所以makefs创建了一个全新的磁盘镜像，在这个磁盘镜像中包含了我们在指令中传入的一些文件。makefs为你创建了一个包含这些文件的新的文件系统。

XV6总是会打印文件系统的一些信息，所以从指令的下方可以看出有46个meta block，其中包括了：

- boot block
- super block
- 30个log block
- 13个inode block
- 1个bitmap block

之后是954个data block。所以这是一个袖珍级的文件系统，总共就包含了1000个block。在File system lab中，你们会去支持更大的文件系统

## Ex: echo "hi" > x

我将print语句放在了log_write中，log_write只能代表文件系统操作的记录，并不能代表实际写磁盘的记录


接下来我们运行一些命令，来看一下特定的命令对哪些block做了写操作，并理解为什么要对这些block写入数据。我们通过echo “hi” > x，来创建一个文件x，并写入字符“hi”。我会将输出拷贝出来，并做分隔以方便我们更好的理解。

如果你去看echo的代码实现，基本就是这3个阶段。
1. open("x", O_CREATE | O_WRONLY | O_TRUNC) : 第一阶段是创建文件
2. write(fd, "hi", 2) : 第二阶段将“hi”写入文件
3. write(fd, "\n", 1) : 第三阶段将“\n”换行符写入到文件

Each of these runs inside a filesystem transaction.


```bash
echo "hi" > x

----create the file
write: 33  # allocate inode for x
write: 33  # init inode x
write: 46  # recode x in / directory's data block
write: 32  # update root inode
write: 33  # update inode x


----write "hi" to the file
write: 45  # set alloc bit in bitmap block
write: 595 # write "h" to allocated data block
write: 595 # write "i" to allocated data block
write: 33  # update inode x: size

----write the newline to the file
write: 595
write: 33
```

### 第一阶段是创建文件 
- write 33（第一次）：占坑 
看起来给我们分配的inode位于block 33。之所以有两个write 33，第一个是为了标记inode将要被使用。
在XV6中，我记得是使用inode中的type字段来标识inode是否空闲，这个字段同时也会用来表示inode是一个文件还是一个目录。所以这里将inode的type从空闲改成了文件，并写入磁盘表示这个inode已经被使用了。

- write 33 （第二次）：把 inode 填完整
  就是实际的写入inode的内容。inode的内容会包含linkcount为1以及其他内容。
  - type = FILE
  - nlink = 1
  - size = 0
  - direct block 先全是 0

- write 46:修改根目录的数据 block
  是向第一个data block写数据，那么这个data block属于谁呢？
  block 46是根目录的第一个block。为什么它需要被写入数据呢？
  文件不是“凭空存在”的， 文件必须出现在某个目录里。
  根目录本身是一个文件： 它的 data block 里存的是 (name, inode number) 对

  因为我们正在向根目录创建一个新文件。这里我们向根目录增加了一个新的entry，其中包含了文件名x，以及我们刚刚分配的inode编号。

- 接下来的write 32又是什么意思呢？根目录 inode 变了
block 32保存的仍然是inode，那么inode中的什么发生了变化使得需要将更新后的inode写入磁盘？是的，根目录的大小变了，因为我们刚刚添加了16个字节的entry来代表文件x的信息。

- write 33（第三次）：回头补写文件 inode
  最后又有一次write 33，我在稍后会介绍这次写入的内容，这里我们再次更新了文件x的inode， 尽管我们又还没有写入任何数据。
  原因是：
  - 在目录中创建 entry 后
  - inode 的 nlink / 状态已经“稳定”
  - 文件正式变成“可见对象”
    这是一次收尾式同步写回，保证 inode 状态完整一致


第一阶段总结一句话：

创建文件不是“造一个空文件”， 而是： 分配 inode → 挂到目录 → 同步所有元数据

### 第二阶段是向文件写入“hi”。

- write 45：更新 bitmap 
  文件系统首先会扫描bitmap来找到一个还没有使用的data block，未被使用的data block对应bit 0。找到之后，文件系统需要将该bit设置为1，表示对应的data block已经被使用了。所以更新block 45是为了更新bitmap。

- write 595（两次）：真正写数据
接下来的两次write 595表明，文件系统挑选了data block 595。所以在文件x的inode中，第一个direct block number是595。因为写入了两个字符，所以write 595被调用了两次。
  - block 595 成了文件 x 的第一个 data block
  - "h"、"i" 写进去
  - 写两次是实现细节（buffer / 系统调用粒度）
  注意：此时 inode 还不知道 size 变了

- write 33：更新文件 inode
  write 33是更新文件x对应的inode中的size字段，因为现在文件x中有了两个字符。
  - size: 0 → 2
  - direct[0] = 595
  也就是说： inode 直到这一步，才“承认”自己有数据

  如果系统在这之前崩溃：
  - bitmap 已经标了
  - data block 里有脏数据
  - 但 inode 没指向它
   文件系统还能回收这块空间

这是防崩溃设计，不是巧合。

### 第三阶段：写入 \n

这一阶段你可以自己推演了：
- 不需要新 block（595 还有空间）
- 直接写 block 595
- inode size 从 2 → 3
- write 33 再次同步 inode



学生提问：block 595看起来在磁盘中很靠后了，是因为前面的block已经被系统内核占用了吗？

Frans教授：我们可以看前面makefs指令，makefs存了很多文件在磁盘镜像中，这些都发生在创建文件x之前，所以磁盘中很大一部分已经被这些文件填满了。

学生提问：第二阶段最后的write 33是否会将block 595与文件x的inode关联起来？

Frans教授：会的。这里的write 33会发生几件事情：首先inode的size字段会更新；第一个direct block number会更新。这两个信息都会通过write 33一次更新到磁盘上的inode中。

以上就是磁盘中文件系统的组织结构的核心，希望你们都能理解背后的原理。


## workflow
```bash
sys_open
 └─ begin_op
 └─ ialloc
 └─ iget
 └─ dirlink
 └─ bwrite (inode, dir, root inode)
 └─ end_op

sys_write("hi")
 └─ begin_op
 └─ balloc
 └─ bwrite(bitmap)
 └─ bwrite(data block)
 └─ bwrite(inode)
 └─ end_op

sys_write("\n")
 └─ begin_op
 └─ bwrite(data block)
 └─ bwrite(inode)
 └─ end_op

```

以上就是第一次写磁盘涉及到的函数调用。这里有个有趣的问题，如果有多个进程同时调用create函数会发生什么？对于一个多核的计算机，进程可能并行运行，两个进程可能同时会调用到 ialloc 函数，然后进而调用bread（block read）函数。所以必须要有一些机制确保这两个进程不会互相影响。





# Crash recovery

我们将会看到很多文件系统的操作都包含了多个步骤，如果我们在多个步骤的错误位置crash或者电力故障了，存储在磁盘上的文件系统可能会是一种不一致的状态，之后可能会发生一些坏的事情。

我们今天会研究对于这类特定问题的解决方法，也就是logging。这是一个最初来自于数据库世界的很流行的解决方案，现在很多文件系统都在使用logging。


## Crash Exampels

### Example 1
```bash
echo "hi" > x

----create the file
write: 33  # allocate inode for x
write: 33  # init inode x

---- power failure here
write: 46  # recode x in / directory's data block
write: 32  # update root inode
write: 33  # update inode x
```

在这个位置，我们先写了block 33表明inode已被使用，之后出现了电力故障，然后计算机又重启了。这时，我们丢失了刚刚分配给文件x的inode。这个inode虽然被标记为已被分配，但是它并没有放到任何目录中，所以也就没有出现在任何目录中，因此我们也就没办法删除这个inode。所以在这个位置发生电力故障会导致我们丢失inode。

你或许会认为，我们应该改一改代码，将写block的顺序调整一下，这样就不会丢失inode了。所以我们可以先写block 46来更新目录内容，之后再写block 32来更新目录的size字段，最后再将block 33中的inode标记为已被分配。

在这个位置，目录被更新了，但是还没有在磁盘上分配inode（有个问题，如果inode没分配的话，write 46的时候写的是啥）。电力故障之后机器重启，文件系统会是一个什么状态？或者说，如果我们读取根目录下的文件x，会发生什么，因为现在在根目录的data block已经有了文件x的记录？

是的，我们会读取一个未被分配的inode，因为inode在crash之前还未被标记成被分配。更糟糕的是，如果inode之后被分配给一个不同的文件，这样会导致有两个应该完全不同的文件共享了同一个inode。如果这两个文件分别属于用户1和用户2，那么用户1就可以读到用户2的文件了。所以上面的解决方案也不好。


### Example 2

```bash
----write "hi" to the file
write: 45  # set alloc bit in bitmap block

----power failure here
write: 595 # write "h" to allocated data block
write: 595 # write "i" to allocated data block
write: 33  # update inode x: size
```

这里我们从bitmap block中分配了一个data block，但是又还没有更新到文件x的inode中。当我们重启之后，磁盘处于一个特殊的状态，这里的风险是什么？是的，我们这里丢失了data block，因为这个data block被分配了，但是却没有出现在任何文件中，因为它还没有被记录在任何inode中。

你或许会想，是因为这里的顺序不对才会导致丢失data block的问题。我们应该先写block 33来更新inode来包含data block 595（同样的问题，这个时候data block都还没有分配怎么知道是595），之后才通过写block 45将data block 595标记为已被分配。

所以，为了避免丢失data block，我们将写block的顺序改成这样。现在我们考虑一下，如果故障发生在这两个操作中间会怎样？

这时inode会认为data block 595属于文件x，但是在磁盘上它还被标记为未被分配的。之后如果另一个文件被创建了，block 595可能会被另一个文件所使用。所以现在两个文件都会在自己的inode中记录block 595。如果两个文件属于两个用户，那么两个用户就可以读写彼此的数据了。很明显，我们不想这样，文件系统应该确保每一个data block要么属于且只属于一个文件，要么是空闲的。所以这里的修改会导致磁盘block在多个文件之间共享的安全问题，这明显是错误的。

所以这里的问题并不在于操作的顺序，而在于我们这里有多个写磁盘的操作，这些操作必须作为一个原子操作出现在磁盘上。


## Logging layer


allows higher layers to wrap updates to several blocks in a transaction, and ensures that the blocks are updated **atomically** in the face of crashes (i.e., all of them are updated or none). 


在logging层之上，XV6有inode cache，这主要是为了同步（synchronization），我们稍后会介绍。inode通常小于一个disk block，所以多个inode通常会打包存储在一个disk block中。为了向单个inode提供同步操作，XV6维护了 inode cache。


What the logging layer is (conceptually)

The logging layer (also called journaling) is a small subsystem that:
- records intent before modifying real filesystem structures
- replays or discards changes after a crash

It does not log file data by default (in xv6).
It logs metadata blocks.

Think of it as:

“Before I change reality, I write down what I’m going to change.”

我们这节课要讨论的针对文件系统crash之后的问题的解决方案，其实就是logging。这是来自于数据库的一种解决方案。它有一些好的属性：
- atomiac fs calls
  比如你调用create/write系统调用，这些系统调用的效果是要么完全出现，要么完全不出现，这样就避免了一个系统调用只有部分写磁盘操作出现在磁盘上。

- Fast Recovery
  在重启之后，我们不需要做大量的工作来修复文件系统，只需要非常小的工作量。这里的快速是相比另一个解决方案来说，在另一个解决方案中，你可能需要读取文件系统的所有block，读取inode，bitmap block，并检查文件系统是否还在一个正确的状态，再来修复。而logging可以有快速恢复的属性。

- High performance
  最后，原则上来说，它可以非常的高效，尽管我们在XV6中看到的实现不是很高效。

## logging的基本思想
logging的基本思想还是很直观的。
首先，你将磁盘分割成两个部分，其中一个部分是log，另一个部分是文件系统，文件系统可能会比log大得多。

- log write
  当需要更新文件系统时，我们并不是更新文件系统本身。
假设我们在内存中缓存了bitmap block，也就是block 45。当需要更新bitmap时，我们并不是直接写block 45，而是将数据写入到log中，并记录这个更新应该写入到block 45。对于所有的写 block都会有相同的操作，例如更新inode，也会记录一条写block 33的log。

所以基本上，任何一次写操作都是先写入到log，我们并不是直接写入到block所在的位置，而总是先将写操作写入到log中。

- commit op
  之后在某个时间，当文件系统的操作结束了，比如说我们前一节看到的4-5个写block操作都结束，并且都存在于log中，我们会commit文件系统的操作。这意味着我们需要在log的某个位置记录属于同一个文件系统的操作的个数，例如5。
  执行commit操作时，你只会在记录了所有的write操作之后，才会执行commit操作。所以在执行commit时，所有的write操作必然都在log中。
  
  而commit操作本身也有个有趣的问题，它究竟会发生什么？
  commit操作本身只会写一个block。文件系统通常可以这么假设，单个block或者单个sector的write是原子操作（注，有关block和sector的区别详见14.3）
  如果你执行写操作，要么整个sector都会被写入，要么sector完全不会被修改。所以sector本身永远也不会被部分写入，并且commit的目标sector总是包含了有效的数据。 
  而commit操作本身只是写log的header
  - 如果它成功了只是在commit header中写入log的长度，例如5，这样我们就知道log的长度为5。这时crash并重启，我们就知道需要重新install 5个block的log。
  - 如果commit header没能成功写入磁盘，那这里的数值会是0。我们会认为这一次事务并没有发生过。
  这里本质上是 **write ahead rule**，它表示logging系统在所有的写操作都记录在log中之前，不能install log。

- install log 
  当我们在log中存储了所有写block的内容时，如果我们要真正执行这些操作，只需要将block从log分区移到文件系统分区。
  我们知道第一个操作该写入到block 45，我们会直接将数据从log写到block45，第二个操作该写入到block 33，我们会将它写入到block 33，依次类推。

- clean log
  一旦完成了，就可以清除log。清除log实际上就是将属于同一个文件系统的操作的个数设置为0。



### 为什么这样的工作方式是好的呢？
假设我们crash并重启了。在重启的时候，文件系统会查看log的commit记录值，
- 如果是0的话，那么什么也不做。
- 如果大于0的话，我们就知道log中存储的block需要被写入到文件系统中，很明显我们在crash的时候并不一定完成了install log，我们可能是在commit之后，clean log之前crash的。所以这个时候我们需要做的就是reinstall（注，也就是将log中的block再次写入到文件系统），再clean log。

这里的方法之所以能起作用，是因为可以确保当发生crash（并重启之后），我们要么将写操作所有相关的block都在文件系统中更新了，要么没有更新任何一个block，我们永远也不会只写了一部分block。

为什么可以确保呢？我们考虑crash的几种可能情况。
- 在第1步和第2步之间crash会发生什么？
在重启的时候什么也不会做，就像系统调用从没有发生过一样，也像crash是在文件系统调用之前发生的一样。这完全可以，并且也是可接受的。

- 在第2步和第3步之间crash会发生什么？
  在这个时间点，所有的log block都落盘了，因为有commit记录，所以完整的文件系统操作必然已经完成了。我们可以将log block写入到文件系统中相应的位置，这样也不会破坏文件系统。所以这种情况就像系统调用正好在crash之前就完成了。

- 在install（第3步）过程中和第4步之前这段时间crash会发生什么？
  在下次重启的时候，我们会redo log，我们或许会再次将log block中的数据再次拷贝到文件系统。这样也是没问题的，因为log中的数据是固定的，我们就算重复写了文件系统，每次写入的数据也是不变的。重复写入并没有任何坏处，因为我们写入的数据可能本来就在文件系统中，所以多次install log完全没问题。当然在这个时间点，我们不能执行任何文件系统的系统调用。我们应该在重启文件系统之前，在重启或者恢复的过程中完成这里的恢复操作。换句话说，install log是幂等操作（注，idempotence，表示执行多次和执行一次效果一样），你可以执行任意多次，最后的效果都是一样的。

## Xv6  logging

Xv6 solves the problem of crashes during file-system operations with a simple form of logging.
An xv6 system call does not directly write the on-disk file system data structures. Instead, it places
a description of all the disk writes it wishes to make in a log on the disk. Once the system call has
logged all of its writes, it writes a special commit record to the disk indicating that the log contains
a complete operation. At that point the system call copies the writes to the on-disk file system data
structures. After those writes have completed, the system call erases the log on disk.

If the system should crash and reboot, the file-system code recovers from the crash as follows,
before running any processes. 
- If the log is marked as containing a complete operation, then the
recovery code copies the writes to where they belong in the on-disk file system. 
- If the log is not marked as containing a complete operation, the recovery code ignores the log. The recovery code finishes by erasing the log.


Why does xv6’s log solve the problem of crashes during file system operations? 
- If the crash occurs before the operation commits, then the log on disk will not be marked as complete, the recovery code will ignore it, and the state of the disk will be as if the operation had not even started. 
- If the crash occurs after the operation commits, then recovery will replay all of the operation’s writes, perhaps repeating them if the operation had started to write them to the on-disk data structure. 

In either case, the log makes operations **atomic** with respect to crashes: after recovery, either all of the operation’s writes appear on the disk, or none of them appear.


## Log design
The log resides at a known fixed location, specified in the `superblock`. 
It consists of a header block followed by a sequence of updated block copies (“logged blocks”). 


// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...


```c
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

```

###  The header block 

  1. The count in the header block on disk is 
  - zero: indicating that there is no transaction in the log, 
  - nonzero: indicating that the log contains a complete committed transaction with the indicated number of logged blocks.

  2.  an array of sector numbers, one for each of the logged blocks

    Xv6 writes the header block when a transaction commits, but not before, and sets the count to zero after copying the logged blocks to the file system. 
  - Thus a crash midway through a transaction will result in a count of zero in the log’s header block; 
  - a crash after a commit will result in a non-zero count.

### logged blocks
之后就是log的数据，也就是每个block的数据，依次为bn0对应的block的数据，bn1对应的block的数据以此类推。这就是log中的内容，并且log也不包含其他内容。

当文件系统在运行时，在内存中也有header block的一份拷贝，拷贝中也包含了n和block编号的数组。这里的block编号数组就是log数据对应的实际block编号，

并且相应的block也会缓存在block cache中，这个在Lec14有介绍过。

与前一节课对应，log中第一个block编号是45，那么在block cache的某个位置，也会有block 45的cache。

   
```c
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};


```

```bash
(gdb) p log
$9 = {
  lock = {
    locked = 1,
    name = 0x8000c5d0 "log",
    cpu = 0x80016490 <cpus+256>,
    nts = 0,
    n = 33
  },
  start = 2,
  size = 30,
  outstanding = 0,
  committing = 0,
  dev = 1,
  lh = {
    n = 0,
    block =       {33,
      47,
      32,
      0 <repeats 27 times>}
  }
}


(gdb) p log.lh
$10 = {
  n = 0,
  block =     {33,
    47,
    32,
    0 <repeats 27 times>}
}

```



# Coding: File system logging

## 实际 Log 写磁盘流程

我已经在bwrite函数中加了一个print语句。bwrite函数是block cache中实际写磁盘的函数，所以我们将会看到实际写磁盘的记录。
在上节课（Lec 14）我将print语句放在了log_write中，log_write只能代表文件系统操作的记录，并不能代表实际写磁盘的记录。我们这里会像上节课一样执行echo "hi" > x，并看一下实际的写磁盘过程。


add logging in bwrite, log_write, write_log, commit

Each transaction obeys strict write-ahead logging rules:
1. Log data blocks
2. Write commit record
3. Install to home locations
4. Clear log



<!-- XV6创建inode代码展示 -->
```bash
# Inside the xv6 shell:
make clean && make qemu
mkfs/mkfs fs.img README  user/_cat user/_echo user/_forktest user/_grep user/_init user/_kill user/_ln user/_ls user/_mkdir user/_rm user/_sh user/_stressfs user/_usertests user/_grind user/_wc user/_zombie  user/_stats user/_kalloctest user/_bcachetest
nmeta 47 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 2) blocks 9953 total 10000
balloc: first 488 blocks have been allocated
balloc: write bitmap block at sector 45
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh



(gdb)
b sys_open
b create
b ialloc
b log_write
b write_log
b commit
b bwrite


- Block 2  ->Log header
- Block 3,4,5 → log data entried
- Block 32 → root directory inode
- Block 33 → inode block containing inode for x
- Block 45 → bitmap
- Block 47 → root directory data block (directory entry for x) 
- Block 488 → data **blocks**

$ echo "hi" > x
start sys_open with path: x and omode: 1537
-----Transaction 1. sys_open: create file x
-----1.1  metadata logging (file creation)

# inode 33 was modified multiple times
# - allocated
# - link count updated
# - size initialized
# but only logged once
[log_write] block 33
[log_write] block 33
[log_write] block 47 # block 47 (data block)  This is the directory data block where the new "x" entry is written.
[log_write] block 32
[log_write] block 33

-----1.2 commit (journal → home locations)
[commit] committing 3 blocks

# Log write phase
# Meaning:
# log[0] ← inode block 33
# log[1] ← directory data block 47
# log[2] ← directory inode block 32
# The bwrite calls are writing log blocks, not filesystem blocks.
[write_log] log block 2 <- fs block 33
[bwrite] block 3          # log data blocks: copies of metadata blocks about to changes
[write_log] log block 3 <- fs block 47
[bwrite] block 4
[write_log] log block 4 <- fs block 32
[bwrite] block 5
# Commit record
# This writes the log header (block 2) saying: # “Transaction committed, 3 blocks valid.”
[bwrite] block 2  # log header updated : declares: “transaction contains blocks X, Y, Z”****


------1.3 Install phase (copy home): 
# Now the real filesystem blocks are updated.
[bwrite] block 33  # real inode block: inode block written to its home location
[bwrite] block 47
[bwrite] block 32

------1.4 Log clear: Header cleared → log empty.
[bwrite] block 2  # log header cleared (commit complete)
# Result so far:
# File x exists, inode allocated, directory updated.
# No file data written yet.

-------Transaction 2. writing 'hi\n'
-----  2.1 writing "hi\n" (data allocation)
[log_write] block 45
[log_write] block 488
[log_write] block 488
[log_write] block 33
# Second commit
[commit] committing 3 blocks
[write_log] log block 2 <- fs block 45
[bwrite] block 3
[write_log] log block 3 <- fs block 488
[bwrite] block 4
[write_log] log block 4 <- fs block 33
[bwrite] block 5
[bwrite] block 2

[bwrite] block 45
[bwrite] block 488
[bwrite] block 33
[bwrite] block 2

-------Transaction 3. writing "\n"
-------3.1 inode update after write finishes
[log_write] block 488
[log_write] block 33
[commit] committing 2 blocks
[write_log] log block 2 <- fs block 488
[bwrite] block 3
[write_log] log block 3 <- fs block 33
[bwrite] block 4
[bwrite] block 2
[bwrite] block 488
[bwrite] block 33
[bwrite] block 2
$
```

## sys_open — the entry point

sys_open() implements the user-level system call:
`int open(char *path, int omode);`
Its job is to:
- find or create the file’s inode
- enforce rules (directories, devices, truncation)
- allocate a file descriptor
- return an integer fd to user space

前面我提过事务（transaction），也就是我们不应该在所有的写操作完成之前写入commit record。这意味着文件系统操作必须表明 事务的开始和结束。

在XV6中，以创建文件的 `sys_open` 为例（在sysfile.c文件中）每个文件系统操作，都有 begin_op 和 end_op 分别表示 transaction 的开始和结束。



Everything starts here: `sys_open`
```bash
sys_open
 └─ begin_op()          ← logging starts
 └─ create()            ← because O_CREATE
 └─ end_op()            ← commit log
```
接下来我们通过查看XV6中的代码，更进一步的了解文件系统。因为我们前面已经分配了inode，我们先来看一下这是如何发生的。sysfile.c 中包含了所有与文件系统相关的函数，分配inode 发生在 `sys_open` 函数中，这个函数会负责创建文件。

```c
// kernel/sysfile.c
uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;
  // This enters the logging layer.
  // All disk writes until end_op() are journaled
  // Ensures filesystem consistency across crashes
  begin_op();

  if(omode & O_CREATE){
    // case1 create: 
    // What create() does:
    // resolves parent directory
    // allocates a new inode
    // inserts a directory entry
    // returns a locked inode
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    // case2: normal open
    // resolves the full path
    // returns the inode (unlocked)
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    //You must lock before inspecting inode fields.
    ilock(ip);
    // Directory write protection
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }
  
  // Devices are special:
  //  major selects the driver
  // If invalid → kernel has no handler
  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  // Allocate a struct file and a file descriptor
  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  // Initialize the file object
  if(ip->type == T_DEVICE){
    // Devices don’t use offsets or data blocks,Regular files do
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }

  // Bind file to inode
  // This creates the open file description:
  // - independent read/write permissions
  // - independent file offset
  // - shared inode

  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  // Handle truncation
  // - frees all data blocks
  // - sets file size to 0
  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  // Cleanup and commit
  iunlock(ip);
  end_op();

  // Return fd to user
  // User now has:
  // an integer index into proc->ofile[]
  // pointing to a live struct file
  return fd;
}

```


```bash
b sys_open
b ialloc
b log_write
b write_log
b bwrite
b commit

(gdb) where
#0  sys_open () at kernel/sysfile.c:301
#1  0x0000000080004926 in syscall () at kernel/syscall.c:140
#2  0x000000008000424a in usertrap () at kernel/trap.c:67
#3  0x00000000000000d8 in ?? ()

(gdb) x/s path
0x3fffff9ef0:   "x"

(gdb) p omode
$72 = 1537

```



## begin_op()
```c
// called at the start of each FS system call.
// log.outstanding counts the number of system calls that have reserved log space; 
// the total reserved space is log.outstanding times MAXOPBLOCKS.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}
```

```bash
(gdb) p log
$11 = {
  lock = {
    locked = 1,
    name = 0x8000c5d0 "log",
    cpu = 0x80016490 <cpus+256>,
    nts = 0,
    n = 33
  },
  start = 2,
  size = 30,
  outstanding = 0,
  committing = 0,
  dev = 1,
  lh = {
    n = 0,
    block =       {33,
      47,
      32,
      0 <repeats 27 times>}
  }
}

#  log.outstanding will increase by 1
(gdb) p log.outstanding
$12 = 1
```

## create()

This is where ialloc, iget, bwrite first appear.

What create() is responsible for: 
- Find the parent directory of path
- Check whether path already exists
- If it exists: Possibly reuse it (for open(O_CREATE))
- If it doesn’t exist:
  - Allocate a new inode
  - Initialize it
  - Insert it into the parent directory

```c
static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  // Find the parent directory
  if((dp = nameiparent(path, name)) == 0)
    return 0;

  // Directories are shared mutable structures.
  ilock(dp);

  // Check if the file already exists:
  // “Is there already an entry called name in directory dp?”
  if((ip = dirlookup(dp, name, 0)) != 0){
    // Case A: file exists
    iunlockput(dp);
    ilock(ip);
    // Caller wants a file, The existing inode is a file or device → reuse it.
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }
  // The file does not exist, create a new inode:  “Give me a fresh inode number on this device.”
  // Finds a free inode in the inode bitmap
  // Marks it allocated
  // Returns a new inode struct
  // No directory entry exists yet. This inode is invisible until linked.
  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  // Initialize the new inode
  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;  // one directory entry will point to it
  iupdate(ip);    // writes inode metadata to disk (journaled)
  // At this moment: 
  // Inode exists on disk, But no directory points to it yet

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    // create the mandatory entries:
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }
  // Link the file into the parent directory
  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");
  // This is the moment of birth: Now the file actually exists.
  // - Directory data block updated
  // - Inode number becomes reachable by name
  iunlockput(dp);
  // The inode is returned locked to the caller (sys_open expects that).
  return ip;
}
```

```bash
2.1 Lookup first (file doesn’t exist)
2.2 ialloc — allocate a new inode
Inside ialloc:

ialloc
 └─ read inode blocks
 └─ find inode with type == 0
 └─ log_write(inode block)
 └─ write type = T_FILE
 └─ bwrite (later via commit)

This corresponds to: write: 33   # allocate inode

2.3 dirlink — add x to root directory

2.4 iget — bring inode x into memory

2.5 End of create transaction
```

create函数中首先会解析路径名并找到最后一个目录，之后会查看文件是否存在，如果存在的话会返回错误。
之后就会调用 `fs.c/ialloc`（inode allocate），这个函数会为文件x分配inode。 
在这个函数中，并没有直接调用 `bwrite` ，这里实际调用的是 `log_write` 函数。
log_write 是由文件系统的logging实现的方法。
任何一个文件系统调用的begin_op和end_op之间的写操作总是会走到log_write。log_write函数位于log.c文件，


```c

```


### Lookup & Pathname Layer

Path name lookup involves a succession of calls to `dirlookup`, one for each path component.

The function `nameiparent` is a variant: it stops before the last element, returning the inode of the parent directory and copying the final element into name. 

`Namei` (kernel/fs.c:664) evaluates path and returns the corresponding inode.

Both call the generalized function `namex` to do the real work.

#### nameiparent
nameiparent(path, name) means:
“Walk the filesystem tree following path, stop at the parent directory,
and copy the final component into name.”

path =/a/b/c
return value → inode of /a/b
name → "c"

```bash
(gdb) where
#0  nameiparent (path=0x3fffff9ef0 "x", name=0x3fffff9eb0 "\340\236\377\377?") at kernel/fs.c:672
#1  0x00000000800085dc in create (path=0x3fffff9ef0 "x", type=2, major=0, minor=0) at kernel/sysfile.c:247
#2  0x0000000080008826 in sys_open () at kernel/sysfile.c:301
#3  0x0000000080004926 in syscall () at kernel/syscall.c:140
#4  0x000000008000424a in usertrap () at kernel/trap.c:67
#5  0x00000000000000d8 in ?? ()


(gdb) p path
$23 = 0x3fffff9ef0 "x"
(gdb) x/s path
0x3fffff9ef0:   "x"

# Before that function writes into it, name contains whatever junk was already on the stack.
(gdb) p name
$24 = 0x3fffff9eb0 "\340\236\377\377?"
(gdb) x/4c name
0x3fffff9eb0:   -32 '\340'      -98 '\236'      -1 '\377'       -1 '\377'



```

#### namex() 

namex() returns a pointer to an in-memory struct inode.
Walk the filesystem tree, directory by directory, and return the inode corresponding to the path.


```c
// kernel/fs.c
struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

// icache is not a single inode
// It’s a container holding:
// - a lock
// - an array of struct inode
// icache:
// +------------------+
// | spinlock         |
// +------------------+
// | inode[0]         |
// | inode[1]         |
// | inode[2]         |
// | ...              |
// +------------------+


// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  // ip is either the root directory inode (/) or the current working directory inode
  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    // Finally, the loop looks for the path element using `dirlookup` and prepares for the next iteration by setting `ip = next` (kernel/fs.c:649-654). 
    // When the loop runs out of path elements, it returns ip.
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}
```

```bash

(gdb) p ip
$1 = (struct inode *) 0x80024bd0 <icache+32>
# That means:
# ip points to one element inside icache.inode[], not to a separately allocated object.
# The +32 is a byte offset from the start of icache (spinlock + padding + maybe earlier inodes).
# This is exactly how iget() works: it returns a pointer to a cached inode entry.


(gdb) p *ip
$10 = {
  dev = 1,
  inum = 1,
  ref = 4,
  lock = {
    locked = 0,
    lk = {
      locked = 0,
      name = 0x8000c620 "sleep lock",
      cpu = 0x0,
      nts = 0,
      n = 24
    },
    name = 0x8000c528 "inode",
    pid = 0
  },
  valid = 1,
  type = 1,
  major = 0,
  minor = 0,
  nlink = 1,
  size = 1024,
  addrs =     {47,
    0 <repeats 12 times>}
}

(gdb) p ip->type
$35 = 1


(gdb) p &icache.inode
$16 = (struct inode (*)[50]) 0x80024bd0 <icache+32>

(gdb) p &icache
$17 = (struct {...} *) 0x80024bb0 <icache>

# That tells you which slot in the inode cache you’re using.
# That number stays stable as long as ref > 0.
(gdb) p (ip-icache.inode)
$18 = 0
```



#### skipelem

skipelem is a pointer-walking path tokenizer that overwrites name with the current path component and returns a pointer to the rest.

s‍kipelem() consumes exactly one path component from path, copies it into name, and returns a pointer to the rest of the path.
```c
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;
  // Skip leading slashes
  while(*path == '/')
    path++;
  //Empty path? Stop.
  // This handles: "", "////" No path element exists.
  if(*path == 0)
    return 0;
  // Mark the start of this path element, s now points at the start of the current filename.
  s = path;
  // Scan until / and !\0
  // s → start of name, path → end of name
  while(*path != '/' && *path != 0)
    // It only moves the pointer to the next character.
    path++;
  // Copy into name
  // name is a caller-provided buffer,It is reused on every call, name is overwritten each time
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  //Skip slashes after this element
  // So "a//bb" → "bb"
  // Return pointer to “rest of path”
  while(*path == '/')
    path++;
  return path;
}

```

#### dirlookup() & Directory layer

```bash
(gdb) p dp->inum
$141 = 1
(gdb) p dp->dev
$142 = 1
(gdb) p dp->size
$143 = 1024
(gdb) p *dp
$144 = {
  dev = 1,
  inum = 1,
  ref = 4,
  lock = {
    locked = 1,
    lk = {
      locked = 0,
      name = 0x8000c620 "sleep lock",
      cpu = 0x0,
      nts = 0,
      n = 37
    },
    name = 0x8000c528 "inode",
    pid = 6
  },
  valid = 1,
  type = 1,
  major = 0,
  minor = 0,
  nlink = 1,
  size = 1024,
  addrs =     {47,
    0 <repeats 12 times>}
}



# if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))) 
(gdb) p de
$124 = {
  inum = 1,
  name =     ".", '\000' <repeats 12 times>
}

(gdb) p de.name
$125 =   ".", '\000' <repeats 12 times>
(gdb) p name
$126 = 0x3fffff9eb0 "x"

```

#### readi

```c
// Read data from inode.
// Caller must hold ip->lock.
// If user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
int
readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return 0;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;
}
```


```bash
# when return tot
(gdb) p tot
$119 = 16
(gdb) p ip
$120 = (struct inode *) 0x80024bd0 <icache+32>
(gdb) p bp
$121 = (struct buf *) 0x8001d110 <bcache+3392>

```


#### bread() & Buffer Cache Layer



```bash
(gdb) where
#0  bget (dev=1, blockno=47) at kernel/bio.c:67
#1  0x0000000080004e0c in bread (dev=1, blockno=47) at kernel/bio.c:97
#2  0x0000000080005e24 in readi (ip=0x80024bd0 <icache+32>, user_dst=0, dst=274877881976,
    off=0, n=16) at kernel/fs.c:467
#3  0x0000000080006138 in dirlookup (dp=0x80024bd0 <icache+32>, name=0x3fffff9eb0 "x",
    poff=0x0) at kernel/fs.c:538
#4  0x000000008000860a in create (path=0x3fffff9ef0 "x", type=2, major=0, minor=0)
    at kernel/sysfile.c:252
#5  0x0000000080008826 in sys_open () at kernel/sysfile.c:301
#6  0x0000000080004926 in syscall () at kernel/syscall.c:140
#7  0x000000008000424a in usertrap () at kernel/trap.c:67
#8  0x00000000000000d8 in ?? ()


(gdb) p b->dev
$107 = 1
(gdb) p dev
$108 = 1
(gdb) p b->blockno
$109 = 2
(gdb) p blockno
$110 = 47
(gdb) p b->refcnt
$113 = 0

(gdb) p b->blockno
$112 = 47

(gdb) p b
$116 = (struct buf *) 0x8001d110 <bcache+3392>
(gdb) p *b
$115 = {
  valid = 1,
  disk = 0,
  dev = 1,
  blockno = 47,
  lock = {
    locked = 1,
    lk = {
      locked = 0,
      name = 0x8000c620 "sleep lock",
      cpu = 0x0,
      nts = 0,
      n = 971
    },
    name = 0x8000c498 "buffer",
    pid = 6
  },
  refcnt = 1,
  prev = 0x8001ccb0 <bcache+2272>,
  next = 0x8001efb0 <bcache+11232>,
  data =     "\001\000.", '\000' <repeats 13 times>, "\001\000..", '\000' <repeats 12 times>, "
\002\000README\000\000\000\000\000\000\000\000\003\000cat", '\000' <repeats 11 times>, "\004\00
0echo\000\000\000\000\000\000\000\000\000\000\005\000forktest\000\000\000\000\000\000\006\000gr
ep\000\000\000\000\000\000\000\000\000\000\a\000init\000\000\000\000\000\000\000\000\000\000\b\
000kill\000\000\000\000\000\000\000\000\000\000\t\000ln", '\000' <repeats 12 times>, "\n\000ls"
, '\000' <repeats 12 times>, "\v\000mkdir\000\000\000\000\000\000\000\000\000\f\000rm", '\000'
<repeats 12 times>...
}
```





### ialloc & Inode Layer

what is ialloc() really doing?
ialloc() finds a free on-disk inode, marks it allocated, writes that fact to disk transactionally, and returns an in-memory inode (struct inode *) that represents it.
Returns an unlocked but allocated and referenced inode.




To allocate a new inode (for example, when creating a file), xv6 calls `ialloc` (kernel/fs.c:196).
Ialloc is similar to `balloc`: it loops over the inode structures on the disk, one block at a time,
looking for one that is marked free. 
When it finds one, it claims it by writing the new type to the disk and then returns an entry from the inode table with the tail call to `iget` (kernel/fs.c:210). 

The correct operation of ialloc depends on the fact that only one process at a time can be holding a reference to `bp`: 
ialloc can be sure that some other process does not simultaneously see that the inode is available and try to claim it.


```c
// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode.
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  // 1. Scan all inodes (yes, linearly)
  // Inode numbers start at 1
  // inum == 0 is reserved / invalid
  for(inum = 1; inum < sb.ninodes; inum++){
    // 2. Read the disk block that contains inode inum
    // IBLOCK(inum, sb) tells you which disk block contains this inode
    // bread() pulls that block into the buffer cache
    bp = bread(dev, IBLOCK(inum, sb));

    // 3. Locate the exact struct dinode inside the block
    // (struct dinode*)bp->data:  treat block data as an array of struct dinode
    // inum % IPB : index of the inode within this block
    // dip points to the on-disk inode
    dip = (struct dinode*)bp->data + inum%IPB;

    // 4️. Is this inode free?
    if(dip->type == 0){  // a free inode
      // 5. Zero it and claim it
      memset(dip, 0, sizeof(*dip));
      // this is the moment of allocation
      dip->type = type;
      
      //6. Write the change transactionally
      log_write(bp);   // mark it allocated on the disk

      // 7. Release the buffer cache entry
      brelse(bp);

      // 8. Return an in-memory inode
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}
```
ialloc() scans the on-disk inode table, finds a free dinode, marks it allocated inside a log transaction, and returns the corresponding in-memory inode.




### log_write & Log Layer

what problem does log_write() solve?

log_write() exists because direct disk writes are dangerous.
So xv6 uses write-ahead logging (WAL):
“Before modifying real filesystem blocks, record what blocks will change in a log.”

```c
// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  // 1. Sanity checks
  // log.lh.n = number of blocks in this transaction
  // LOGSIZE = compile-time max
  // log.size = runtime log size from superblock
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  // We’re about to mutate shared log state.
  acquire(&log.lock);

  // Log absorption: “Have we already logged this block in this transaction?”
  // Think of log.lh.block[] as the set of disk blocks that this transaction has dirtied.
  // log.lh.n = how many distinct blocks are already part of the current transaction.
  // log.lh.block[0 .. n-1] = their block numbers.
  // multiple modifications to the same block collapse into a single log entry.

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }

  // Record the block number
  log.lh.block[i] = b->blockno;
  
  if (i == log.lh.n) {  // Add new block to log?
    // Case A: new block
    // We append a new entry
    // bpin(b):
    // increments the buffer’s refcount
    // prevents eviction from the buffer cache so it can’t be evicted before commit
    bpin(b);    
    log.lh.n++;  // extend the set
  }
  release(&log.lock);
}

```

log_write还是很简单直观的，我们已经向 block cache 中的某个block写入了数据。比如写block 45，我们已经更新了block cache中的block 45。接下来我们需要在内存中记录，在稍后的commit中，要将block 45写入到磁盘的log中。

这里的代码先获取log header的锁，之后再更新log header。
首先代码会查看block 45是否已经被log记录了。
- 如果是的话，其实不用做任何事情，因为block 45已经会被写入了。这种忽略的行为称为log absorbtion
- 如果block 45不在需要写入到磁盘中的block列表中，接下来会对n加1，并将block 45记录在列表的最后。

#### bpin

之后，这里会通过调用 `bpin` 函数将block 45固定在block cache中，我们稍后会介绍为什么要这么做（注，详见15.8）。

以上就是log_write的全部工作了。任何文件系统调用，如果需要更新block或者说更新block cache中的block，都会将block编号加在这个内存数据中（注，也就是log header在内存中的cache），除非编号已经存在。
```bash
void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}

```


学生提问：这是不是意味着，bwrite不能直接使用？

Frans教授：是的，可以这么认为，文件系统中的所有bwrite都需要被log_write替换。

### iget

what is iget() for?
iget(dev, inum) returns the unique in-memory representative of “inode number inum on device dev”.

Iget (kernel/fs.c:243) looks through the inode table for an active entry (`ip->ref > 0`) with
the desired device and inode number. 
If it finds one, it returns a new reference to that inode (kernel/fs.c:252-256). 
As iget scans, it records the position of the first empty slot (kernel/fs.c:257-258), which it uses if it needs to allocate a table entry.


iget():
- finds or creates a struct inode in the inode cache (icache)
- increments its reference count
- returns it unlocked

Invariant xv6 wants to maintain:
For any (dev, inum), there is at most one struct inode in memory.

iget() enforces that invariant.


```c
// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;
  // 1.Lock the inode cache
  acquire(&icache.lock);

  // 2. Scan the cache
  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    // Case A: inode already cached
    // ref > 0 → slot is in use
    // (dev, inum) uniquely identifies a disk inode
    // Increment ref → another user now holds it
    
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      // Notice: No disk access, No locking of the inode itself, Just reference counting
      // This is why:  two processes opening the same file, end up sharing the same struct inode
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    // First, try to find the inode.
    // Only if it doesn’t exist, reuse an empty slot.
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Case B: inode not cached → recycle a slot
  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  //Initialize the new cache entry
  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;  //“I haven’t read the on-disk inode yet.”

  // Release cache lock and return
  release(&icache.lock);
  // At this point:
  // You have a struct inode *
  // It is referenced
  // It is not locked
  // It is possibly invalid
  return ip;
}
```

iget() ensures there is exactly one in-memory inode per (dev, inum), bumps its reference count, and postpones all real work until ilock().


Q: Why iget() does not lock the inode
Because locking implies sleeping, and:
- iget() is often called while holding other locks
- sleeping here would invite deadlocks

Instead, xv6 uses a two-phase protocol:
- iget() → identity + refcount
- ilock() → data + disk I/O

You’ll often see this pattern:

```c
ip = iget(dev, inum);
ilock(ip);

```
That separation is one of xv6’s cleanest design choices.

Q: Why iget() does not read from disk
Because:
- not every inode access needs disk data
- path traversal needs identity before data
- caching works best when reads are delayed

#### ilock
Disk reads happen in ilock():
```c
if(ip->valid == 0)
  read dinode from disk
```
xv6 does:
- read the dinode from disk
- copy it into inode
- set valid = 1



```bash
(gdb) p *ip
$57 = {
  dev = 1,
  inum = 22,
  ref = 1,
  lock = {
    locked = 0,
    lk = {
      locked = 0,
      name = 0x8000c620 "sleep lock",
      cpu = 0x0,
      nts = 0,
      n = 9
    },
    name = 0x8000c528 "inode",
    pid = 0
  },
  valid = 1,
  type = 3,
  major = 1,
  minor = 0,
  nlink = 1,
  size = 0,
  addrs =     {0 <repeats 13 times>}
}

(gdb) p ip->ref
$58 = 1
(gdb) p ip->dev
$59 = 1
(gdb) p ip->inum
$60 = 22

(gdb) p *empty
$65 = {
  dev = 1,
  inum = 4,
  ref = 0,
  lock = {
    locked = 0,
    lk = {
      locked = 0,
      name = 0x8000c620 "sleep lock",
      cpu = 0x0,
      nts = 0,
      n = 3
    },
    name = 0x8000c528 "inode",
    pid = 0
  },
  valid = 1,
  type = 2,
  major = 0,
  minor = 0,
  nlink = 1,
  size = 16896,
  addrs =     {70,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82}

```

#### iupdate



## end_op函数

可以看到，即使是这么简单的一个文件系统也有一些微秒的复杂之处，代码的最开始就是一些复杂情况的处理（注，15.8有这部分的解释）。我直接跳到正常且简单情况的代码。在简单情况下，没有其他的文件系统操作正在处理中。这部分代码非常简单直观，首先调用了commit函数。让我们看一下commit函数的实现，


### commit函数

commit中有两个操作：

#### write_log
首先是 write_log 。
  这基本上就是将所有存在于内存中的log header中的block编号对应的block，从block cache写入到磁盘上的log区域中（注，也就是将变化先从内存拷贝到log中）。
  函数中依次遍历log中记录的block，并写入到log中。它首先读出log block，将cache中的block拷贝到log block，最后再将log block写回到磁盘中。这样可以确保需要写入的block都记录在log中。但是在这个位置，我们还没有commit，现在我们只是将block存放在了log中。如果我们在这个位置也就是在write_head之前crash了，那么最终的表现就像是transaction从来没有发生过。


what phase is write_log()?

What the log looks like on disk?
```
log.start           log.start + log.size - 1
   |                       |
   v                       v
+--------+--------+--------+--------+ ...
| header | log[0] | log[1] | log[2] |
+--------+--------+--------+--------+
```
Important facts:
- log.start = first block number of the log region
- log.start itself holds the log header
- Actual logged data blocks start at log.start + 1
- log.lh.block[i] tells you which filesystem block is stored in log slot i


```c
// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;
  // log.lh.n = number of distinct blocks modified

  for (tail = 0; tail < log.lh.n; tail++) {
    printf("[write_log] log block %d <- fs block %d\n", log.start + tail+1, log.lh.block[tail]);
    
    // log.start is: The disk block number where the log region begins.
    // It is set during filesystem initialization from the superblock.
    
    // Destination: log.start + tail + 1 → log block
    // why +1 : log.start is reserved for the log header
    // Source: log.lh.block[tail] → filesystem block  
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block

    // this is the heart of write-ahead logging: Copy the new filesystem data into the log.
    // Not metadata. Not a diff. The entire block.
    memmove(to->data, from->data, BSIZE);

    // Now the critical durability step:
    // The log block is written to disk
    // After this point, the update is recoverable after a crash
    bwrite(to);  // write the log
   

    //Release both buffers back to the cache.
    //Pinned buffers stay pinned; this only drops the local references.
    brelse(from);
    brelse(to);
  }
}

```

#### write_head


接下来看一下write_head函数，我之前将 write_head 称为 commit point。

会将内存中的log header写入到磁盘中。 

函数也比较直观，首先读取log的header block。将n拷贝到block中，将所有的block编号拷贝到header的列表中。最后再将header block写回到磁盘。函数中的倒数第2行，bwrite是实际的commit point吗？如果crash发生在这个bwrite之前，会发生什么？

这时虽然我们写了log的header block，但是数据并没有落盘。所以crash并重启恢复时，并不会发生任何事情。那crash发生在bwrite之后会发生什么呢？

这时header会写入到磁盘中，当重启恢复相应的文件系统操作会被恢复。在恢复过程的某个时间点，恢复程序可以读到log header并发现比如说有5个log还没有install，恢复程序可以将这5个log拷贝到实际的位置。所以这里的bwrite就是实际的commit point。在commit point之前，transaction并没有发生，在commit point之后，只要恢复程序正确运行，transaction必然可以完成。


```c
// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  // Interpret the raw block bytes as a struct logheader.
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  // Copy the list of filesystem block numbers into the on-disk header.
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }

  // This single call is the commit point.
  // It writes the log header to disk atomically (one block).
  // This buf is the log header block on disk, located at:  block number = log.start
  // The header fits in one disk block. That gives you an atomic guarantee:
  // Either the old header is there Or the new header is there ，Never half a header
  bwrite(buf);
  brelse(buf);
}
```

函数也比较直观，首先读取log的header block。将n拷贝到block中，将所有的block编号拷贝到header的列表中。最后再将header block写回到磁盘。函数中的倒数第2行，bwrite是实际的commit point吗？如果crash发生在这个bwrite之前，会发生什么？

这时虽然我们写了log的header block，但是数据并没有落盘。所以crash并重启恢复时，并不会发生任何事情。那crash发生在bwrite之后会发生什么呢？

这时header会写入到磁盘中，当重启恢复相应的文件系统操作会被恢复。在恢复过程的某个时间点，恢复程序可以读到log header并发现比如说有5个log还没有install，恢复程序可以将这5个log拷贝到实际的位置。所以这里的bwrite就是实际的commit point。在commit point之前，transaction并没有发生，在commit point之后，只要恢复程序正确运行，transaction必然可以完成。

Q: why do you say "bwrite(buf);" is the the commit point?
bwrite(buf) is the commit point because it writes the log header to disk, not because it writes “some buffer”.
That one write makes the transaction discoverable after a crash. That’s what “commit” means here.

Q: Why is write_head() the true commit point?

Because crash recovery logic keys off the log header.
On boot, recovery code does roughly this:
1. Read log header at log.start
2. If n == 0 → nothing to do
3. If n > 0 → a committed transaction exists, Copy log blocks to home locations
4. Clear the log

After write_head(): The transaction is officially committed


!: Why is this safe?
Because of ordering:
1. write_log() All log data blocks are written
2. write_head() One small block marks the transaction valid

If a crash happens:
- Before step 1 finishes
  No header → nothing committed
- After step 1 but before step 2
  Data exists, but header says nothing → ignored
- After step 2
  Header exists → recovery replays transaction

That’s classic write-ahead logging discipline: Data before metadata.


回到commit函数，在commit point之后，就会实际应用transaction。这里很直观，就是读取log block再查看header这个block属于文件系统中的哪个block，最后再将log block写入到文件系统相应的位置。让我们看一下install_trans函数，



### install_trans函数

这里先读取log block，再读取文件系统对应的block。将数据从log拷贝到文件系统，最后将文件系统block缓存落盘。这里实际上就是将block数据从log中拷贝到了实际的文件系统block中。当然，可能在这里代码的某个位置会出现问题，但是这应该也没问题，因为在恢复的时候，我们会从最开始重新执行过。

在commit函数中，install结束之后，会将log header中的n设置为0，再将log header写回到磁盘中。将n设置为0的效果就是清除log。



# Coding: File system recovering

接下来我们看一下发生在XV6的启动过程中的文件系统的恢复流程。当系统crash并重启了，在XV6启动过程中做的一件事情就是调用initlog函数。

## initlog
initlog 基本上就是调用recover_from_log函数。

```c
void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}


```
## recover_from_log

recover_from_log先调用read_head函数从磁盘中读取header，之后调用install_trans函数。这个函数之前在commit函数中也调用过，它就是读取log header中的n，然后根据n将所有的log block拷贝到文件系统的block中。recover_from_log在最后也会跟之前一样清除log。

这就是恢复的全部流程。如果我们在install_trans函数中又crash了，也不会有问题，因为之后再重启时，XV6会再次调用initlog函数，再调用recover_from_log来重新install log。如果我们在commit之前crash了多次，在最终成功commit时，log可能会install多次。


# File system challenges


## 1. cache eviction
第一个是cache eviction。假设transaction还在进行中，我们刚刚更新了block 45，正要更新下一个block，而整个buffer cache都满了并且决定撤回block 45。在buffer cache中撤回block 45意味着我们需要将其写入到磁盘的block 45位置，这里会不会有问题？
如果我们这么做了的话，会破坏什么规则吗？
是的，如果将block 45写入到磁盘之后发生了crash，就会破坏transaction的原子性。这里也破坏了前面说过的write ahead rule，write ahead rule的含义是，你需要先将所有的block写入到log中，之后才能实际的更新文件系统block。所以buffer cache不能撤回任何还位于log的block。

前面在介绍log_write函数时，其中调用了一个叫做bpin的函数，这个函数的作用就如它的名字一样，将block固定在buffer cache中。它是通过给block cache增加引用计数来避免cache撤回对应的block。在之前（注，详见14.6）我们看过，如果引用计数不为0，那么buffer cache是不会撤回block cache的。相应的在将来的某个时间，所有的数据都写入到了log中，我们可以在cache中unpin block（注，在15.5中的install_trans函数中会有unpin，因为这时block已经写入到了log中）。所以这是第一个复杂的地方，我们需要pin/unpin buffer cache中的block。



## 2. max log size
第二个挑战是，文件系统操作必须适配log的大小。
在XV6中，总共有30个log block（注，详见14.3）。当然我们可以提升log的尺寸，在真实的文件系统中会有大得多的log空间。但是无所谓啦，不管log多大，文件系统操作必须能放在log空间中。如果一个文件系统操作尝试写入超过30个block，那么意味着部分内容需要直接写到文件系统区域，而这是不被允许的，因为这违背了write ahead rule。所以所有的文件系统操作都必须适配log的大小。

为什么XV6的log大小是30？因为30比任何一个文件系统操作涉及的写操作数都大，Robert和我看了一下所有的文件系统操作，发现都远小于30，所以就将XV6的log大小设为30。我们目前看过的一些文件系统操作，例如创建一个文件只包含了写5个block。实际上大部分文件系统操作只会写几个block。

你们可以想到什么样的文件系统操作会写很多很多个block吗？
是的，写一个大文件。如果我们调用write系统调用并传入1M字节的数据，这对应了写1000个block，这看起来会有很严重的问题，因为这破坏了我们刚刚说的“文件系统操作必须适配log的大小”这条规则。

```c
// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}

```
从这段代码可以看出，如果写入的block数超过了30，那么一个写操作会被分割成多个小一些的写操作。这里整个写操作不是原子的，但是这还好啦，因为write系统调用的语义并不要求所有1000个block都是原子的写入，它只要求我们不要损坏文件系统。所以XV6会将一个大的写操作分割成多个小的写操作，每一个小的写操作通过独立的transaction写入。这样文件系统本身不会陷入不正确的状态中。

这里还需要注意，因为block在落盘之前需要在cache中pin住，所以buffer cache的尺寸也要大于log的尺寸。


## 3. concurrent fs calls

最后一个要讨论的挑战是并发文件系统调用。让我先来解释一下这里会有什么问题，再看对应的解决方案。假设我们有一段log，和两个并发的执行的transaction，其中transaction t0在log的前半段记录，transaction t1在log的后半段记录。可能我们用完了log空间，但是任何一个transaction都还没完成。

现在我们能提交任何一个transaction吗？我们不能，因为这样的话我们就提交了一个部分完成的transaction，这违背了write ahead rule，log本身也没有起到应该的作用。所以必须要保证多个并发transaction加在一起也适配log的大小。所以当我们还没有完成一个文件系统操作时，我们必须在确保可能写入的总的log数小于log区域的大小的前提下，才允许另一个文件系统操作开始。

XV6通过限制并发文件系统操作的个数来实现这一点。在begin_op中，我们会检查当前有多少个文件系统操作正在进行。如果有太多正在进行的文件系统操作，我们会通过sleep停止当前文件系统操作的运行，并等待所有其他所有的文件系统操作都执行完并commit之后再唤醒。这里的其他所有文件系统操作都会一起commit。有的时候这被称为 **group commit**，因为这里将多个操作像一个大的transaction一样提交了，这里的多个操作要么全部发生了，要么全部没有发生。


Each system call’s code indicates the start and end of the sequence of writes that must be **atomic** with respect to crashes. 
To allow concurrent execution of file-system operations by different processes, the logging system can accumulate the writes of multiple system calls into one transaction.
Thus a single commit may involve the writes of multiple complete system calls. 
To avoid splitting a system call across transactions, the logging system only commits when no file-system system calls are underway.

### group commit

The idea of committing several transactions together is known as **group commit**. 
Group commit reduces the number of disk operations because it amortizes the fixed cost of a commit over multiple operations. 
Group commit also hands the disk system more concurrent writes at the same time, perhaps allowing the disk to write them all during a single disk rotation. 

Xv6’s virtio driver doesn’t support this kind of batching, but xv6’s file system design allows for it.


Xv6 dedicates a fixed amount of space on the disk to hold the log. The total number of blocks written by the system calls in a transaction must fit in that space. This has two consequences. 
No single system call can be allowed to write more distinct blocks than there is space in the log.
This is not a problem for most system calls, but two of them can potentially write many blocks:
write and unlink. 

A large file write may write many data blocks and many bitmap blocks as well as an inode block; 
unlinking a large file might write many bitmap blocks and an inode. 

Xv6’s write system call breaks up large writes into multiple smaller writes that fit in the log, and unlink doesn’t cause problems because in practice the xv6 file system uses only one bitmap block. The other consequence of limited log space is that the logging system cannot allow a system call to start unless it is certain that the system call’s writes will fit in the space remaining in the log.


最后我们再回到最开始，看一下begin_op，
首先，如果log正在commit过程中，那么就等到log提交完成，因为我们不能在install log的过程中写log；其次，如果当前操作是允许并发的操作个数的后一个，那么当前操作可能会超过log区域的大小，我们也需要sleep并等待所有之前的操作结束；最后，如果当前操作可以继续执行，需要将log的outstanding字段加1，最后再退出函数并执行文件系统操作。

再次看一下end_op函数，

在最开始首先会对log的outstanding字段减1，因为一个transaction正在结束；其次检查committing状态，当前不可能在committing状态，所以如果是的话会触发panic；如果当前操作是整个并发操作的最后一个的话（log.outstanding == 0），接下来立刻就会执行commit；如果当前操作不是整个并发操作的最后一个的话，我们需要唤醒在begin_op中sleep的操作，让它们检查是不是能运行。

（注，这里的outstanding有点迷，它表示的是当前正在并发执行的文件系统操作的个数，MAXOPBLOCKS定义了一个操作最大可能涉及的block数量。在begin_op中，只要log空间还足够，就可以一直增加并发执行的文件系统操作。所以XV6是通过设定了MAXOPBLOCKS，再间接的限定支持的并发文件系统操作的个数）

所以，即使是XV6中这样一个简单的文件系统，也有一些复杂性和挑战。

最后让我总结一下：

这节课讨论的是使用logging来解决crash safety或者说多个步骤的文件系统操作的安全性。这种方式对于安全性来说没有问题，但是性能不咋地。


# Disk layer 
reads and writes blocks on an virtio hard drive. 




# Buffer Cache layer 

caches disk blocks and synchronizes access to them, making sure that only one kernel process at a time can modify the data stored in any particular block. 

这些cache可以避免频繁的读写磁盘。这里我们将磁盘中的数据保存在了内存中。

The buffer cache has two jobs: 
- (1) synchronize access to disk blocks to ensure that only one copy of a block is in memory and that only one kernel thread at a time uses that copy;
- (2) cache popular blocks so that they don’t need to be re-read from the slow disk. 
  

## The buffer cache Structure

The buffer cache has a fixed number of buffers to hold disk blocks, which means that if the file system asks for a block that is not already in the cache, the buffer cache must recycle a buffer currently holding some other block. The buffer cache recycles the least recently used buffer for the new block. The assumption is that the least recently used buffer is the one least likely to be used again soon.

What the buffer cache is (mentally)
The buffer cache is:
- A fixed array of NBUF buffers
- Each buffer represents one disk block
- Buffers are reused over time
- Only unused buffers may be recycled
- Recency is tracked by a doubly linked list (LRU)

```c

// A buffer has two state fields associated with it. 
// The field valid indicates that the buffer contains a copy of the block. 
// The field disk indicates that the buffer content has been handed to the disk, which may change the buffer (e.g., write data from the disk into data).
struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};


struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent
  // head.prev is least.
  struct buf head;
} bcache;
```

## binit

The buffer cache is a doubly-linked list of buffers. 
The function `binit`, called by main (kernel/-main.c:27), initializes the list with the `NBUF` buffers in the static array buf (kernel/bio.c:43-52). 
All other access to the buffer cache refer to the linked list via `bcache.head`, not the buf array. 


```c

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;

  // building the LRU list
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    //  insert after head
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}
```


The code is in bio.c.
The main interface exported by the buffer cache consists of `bread` and `bwrite`; 
- `bread`:  obtains a buf containing a copy of a block which can be read or modified in memory, and
- `bwrite`:  writes a modified buffer to the appropriate block on the disk. 

the former obtains a buf containing a copy of a block which can be read or modified in memory, and the latter writes a modified buffer to the appropriate block on the disk. 

A kernel thread must release a buffer by calling `brelse` when it is done with it. 
The buffer cache uses a per-buffer sleep-lock to ensure that only one thread at a time uses each buffer (and thus each disk block); bread returns a locked buffer, and `brelse` releases the lock.


`Bread` (kernel/bio.c:93) calls `bget` to get a buffer for the given sector (kernel/bio.c:97). If the buffer needs to be read from disk, bread calls `virtio_disk_rw` to do that before returning the buffer.

`Bget` (kernel/bio.c:59) scans the buffer list for a buffer with the given device and sector numbers (kernel/bio.c:65-73). If there is such a buffer, bget acquires the sleep-lock for the buffer. Bget then returns the locked buffer.


## bread & bget

首先看一下 bread函数
bread函数首先会调用bget函数，bget会为我们从buffer cache中找到block的缓存。让我们看一下bget函数
```c
// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);
  // You are now allowed to:
  // walk the LRU list
  // inspect refcnt
  // decide ownership


  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    // Search by identity (dev, blockno)
    // The identity of the buffer is protected by bcache.lock
    // The contents are protected by b->lock
    if(b->dev == dev && b->blockno == blockno){
      // If found:
      b->refcnt++;            // Increase refcnt → pin buffer in memory

      // Once refcount is incremented, the buffer cannot be recycled, so it’s safe to drop bcache.lock.
      release(&bcache.lock);  // Drop global lock immediately

      acquiresleep(&b->lock); // Acquire buffer’s sleeplock
      // Why this order matters:
      // - Never sleep while holding a spinlock 
      // - Never let two threads modify data[] simultaneously
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // Walk from least recently used
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    // A buffer is reused for a different block 
    // (A buffer becomes unused in brelse, refcnt drops to zero,Buffer is placed at the head of the LRU list )
    // Only reuse buffers with refcnt == 0
    // That buffer is evicted and reassigned . Never evict an in-use buffer
    // Eviction happens in bget, under bcache.lock, by reusing a buffer whose refcnt == 0.
    if(b->refcnt == 0) {
      b->dev = dev;        // Identity is reassigned
      b->blockno = blockno;
      b->valid = 0;        // When recycled: Old contents are forgotten (valid = 0)
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}


```
遍历了linked-list，来看看现有的cache是否符合要找的block。
是的，我们这里看一下block 33的cache是否存在，如果存在的话，将block对象的引用计数（refcnt）加1，之后再释放bcache锁，因为现在我们已经完成了对于cache的检查并找到了block cache。之后，代码会尝试获取block cache的锁。

## 多个进程同时调用 bget

如果有多个进程同时调用 bget 的话，其中一个可以获取bcache的锁并扫描 buffer cache。
此时，其他进程是没有办法修改buffer cache的（注，因为bcache的锁被占住了）。
之后，进程会查找block number是否在cache中，如果在的话将block cache的引用计数 refcnt 加1，表明当前进程对block cache有引用，之后再释放bcache的锁。

如果有第二个进程也想扫描 buffer cache，那么这时它就可以获取bcache的锁。
假设第二个进程也要获取block 33的cache，那么它也会对相应的block cache的引用计数加1。

最后这两个进程都会尝试对block 33的block cache调用 `acquiresleep` 函数。

### bcache.lock &  acquiresleep

#### Why two locks?

Why one lock is not enough?
If you used only bcache.lock
- You would hold it during disk I/O
- Every disk read/write would block the entire cache
- System becomes single-threaded
- Performance collapses

If you used only b->lock
- Two CPUs could allocate the same buffer
- LRU list would corrupt
- refcnt races
- Kernel explodes quietly and later

So xv6 uses both, carefully layered.

Because:
- Metadata contention should spin briefly
- Data access may block for milliseconds
If you used a spinlock for disk I/O, the CPU would burn cycles staring into the void. Bad form.

```c
// Spinlock: bcache.lock
struct spinlock lock;

// Sleeplock: b->lock
struct sleeplock lock;
```

#### bcache.lock - Spinlock(short, global, structural)
Purpose: Protects the buffer cache data structure itself, it protects metadata, not data.
What it protects:
  - LRU linked list (prev / next)
  - refcnt
  - (dev, blockno) identity
What it does not protect:
- the contents of b->data
- disk I/O

Why it’s a spinlock:
- Held for very short time
- No sleeping allowed
- Only pointer manipulations and counters


```c
// Spin (busy wait)
// The CPU repeatedly checks:
while(lock is held) {
  do nothing;
}

// This:
// burns CPU cycles
// prevents other work
// is only tolerable for very short waits
```

#### b->lock - Sleeplock(long, per-buffer, data-level)
Purpose: Protects the contents of one specific block
What it protects:
- b->data: data[]
- disk read/write ownership (b->disk, b->valid)

Held while:
- Reading from disk
- Writing to disk
- Modifying block data

Why it’s a sleep lock:
- Disk I/O may take milliseconds
- Kernel must sleep, not spin
- Many processes can wait for the same block


#### Why disk I/O makes spinning unacceptable

Disk I/O is slow on CPU timescales:
| Operation        | Typical latency     |
| ---------------- | ------------------- |
| Spinlock section | tens of nanoseconds |
| Context switch   | microseconds        |
| Disk I/O         | **milliseconds**    |


A spinlock does exactly what the name says:
- If the lock is busy, the CPU spins in a loop
- Interrupts are disabled
- The thread cannot sleep
- The CPU does no useful work

That’s acceptable only if:
- The critical section is very short
- The holder will release the lock quickly

Disk I/O violates this brutally.

Disk I/O means:
- You submit a request
- You wait for an interrupt
- That might take milliseconds
- Milliseconds = millions of CPU cycles

If you held a spinlock while waiting for disk:
- One CPU spins uselessly
- Other CPUs may block on the same lock
- The system crawls or deadlocks

Sleep lock reality

A sleep lock does something smarter:
- If the lock is busy:
  - The thread goes to sleep
  - The CPU is free to run something else
- When the lock is released:
  - Sleeping threads are woken up

So:
- Long waits are fine
- Many waiters are fine
- CPU time is not wasted

That’s why:

- buf.lock is a sleep lock
- It is held across disk I/O
- It protects buffer contents, not metadata





Milliseconds is millions of CPU cycles.
If you spin during disk I/O:
- you waste a whole core
- nothing else runs on that CPU
- performance collapses under load
That’s why xv6 forbids sleeping while holding a spinlock


#### acquiresleep & sleep lock

acquiresleep 是另一种锁，我们称之为 **sleep lock**，本质上来说它获取block 33 cache的锁。其中一个进程获取锁之后函数返回。在ialloc函数中会扫描block 33中是否有一个空闲的inode。而另一个进程会在acquiresleep中等待第一个进程释放锁。

What acquiresleep() really means?
sleeplock is a blocking lock.

```c
If lock is free:
    take it
If thread A holds b->lock and thread B wants it:
- Thread B sleeps
- scheduler runs someone else
- Thread B wakes when lock is released
```

This is perfect for:
- Safe for long waits (disk I/O)
- CPU is not wasted spinning
- Many waiters allowed

That’s why:
- Spinlocks protect structure
- Sleeplocks protect substance




首先XV6中对bcache做任何修改的话，都必须持有 bcache 的锁；
其次对block 33的cache做任何修改你需要持有block 33的sleep lock。
所以在任何时候，release(&bcache.lock)之后，b->refcnt都大于0。

block的cache只会在refcnt为0的时候才会被驱逐，任何时候refcnt大于0都不会驱逐block cache。
所以当b->refcnt大于0的时候，block cache本身不会被buffer cache修改。
这里的第二个锁，也就是block cache的sleep lock，是用来保护block cache的内容的。它确保了任何时候只有一个进程可以读写block cache。


#### releasesleep()
When releasesleep() happens:
- one (or more) sleeping waiters are woken
- they compete to acquire the lock
No CPU is wasted while waiting.

#### The lock ordering rule (this is critical)

"Always acquire bcache.lock before b->lock"

It is safe for bget to acquire the buffer’s sleep-lock outside of the `bcache.lock` critical section, since the non-zero `b->refcnt` prevents the buffer from being re-used for a different disk block. 
The sleep-lock protects reads and writes of the block’s buffered content, while the bcache.lock protects information about which blocks are cached.

## brelse

What brelse() means conceptually ?
Calling brelse(b) means:
“I’m done — if nobody else needs me, remember me as recently useful, but feel free to reuse me later.”

The LRU position is updated in brelse()


```c
// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  //First rule: you must own the buffer
  //If you touch b->data, you must hold b->lock.
  //If you release a buffer, you must still hold that lock.
  //Because otherwise:
  // One thread could still be modifying the buffer
  // Another thread could recycle it
  // Silent data corruption follows

  if(!holdingsleep(&b->lock))
    panic("brelse");
  
  // Release the buffer’s lock first
  // This allows:
  // Other threads waiting on this block to proceed
  // Disk I/O to continue elsewhere
  releasesleep(&b->lock);

  acquire(&bcache.lock);
  // you’re done touching the data,  you update cache metadata
  // LRU update is structural → spinlock

  b->refcnt--;


  // A buffer becomes “recently used” when the last user finishes with it.
  // LRU only matters for eviction, not for active use
  if (b->refcnt == 0) {
    // A buffer becomes unused
    // refcnt == 0: buffer is now free， no one is waiting for it.
    // How the LRU move actually works
    // 1: unlink b from current position
    // This removes b from wherever it currently is in the list.
    b->next->prev = b->prev;
    b->prev->next = b->next;

    // 2: insert b at the MRU position,which means right after head.
    // head.next  → most recently used (MRU)
    // head.prev  → least recently used (LRU)

    // Before insertion, the list looks like this:
    // head <-> A <-> ...
    // After insertion, the list looks like this:
    // head <-> b <-> A <-> old_MRU <-> ...
    
    // That means four pointer relationships must be correct:
    // b.next == A
    // b.prev == &head
    // A.prev == b
    // head.next == b

    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}
```

以上就是对于 block cache 代码的介绍。这里有几件事情需要注意：
1. 首先在内存中，对于一个block只能有一份缓存。这是block cache必须维护的特性。
2. 其次，这里使用了与之前的spinlock略微不同的sleep lock。
   与spinlock不同的是，可以在I/O操作的过程中持有sleep lock。
3. 采用了LRU作为cache替换策略。
4. 它有两层锁。第一层锁用来保护buffer cache的内部数据；第二层锁也就是sleep lock用来保护单个block的cache。






# Directory layer 

implements each directory as a special kind of inode whose content is a sequence of **directory entries**, each of which contains a file’s name and i-number. 

A directory is implemented internally much like a file. 
- Its inode has type `T_DIR` and
- its data is a sequence of directory entries. 
  Each entry is a `struct dirent` (kernel/fs.h:56), which contains a name and an inode number. 
  - The name is at most DIRSIZ (14) characters; if shorter, it is terminated by a NUL (0) byte. 

Directory entries with inode number zero are free.


```c
// kernel/file.h
// in-memory copy of an inode
struct inode {
  uint dev;           // Device number : (dev, inum) → dinode
  uint inum;          // Inode number
  int ref;            // Reference count : How many kernel users currently hold this inode
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode: These are a cached copy of the dinode.
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};


// A directory in xv6 is just a file whose data blocks contain an array of `struct dirent`:
// A directory is a flat file of (name → inode number) records.
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

```

## 目录（directory）

文件系统的酷炫特性就是层次化的命名空间（hierarchical namespace），你可以在文件系统中保存对用户友好的文件名。

大部分Unix文件系统有趣的点在于
- 一个目录本质上是一个文件， 加上一些文件系统能够理解的结构。
- 目录不是“树结构”，而是“普通文件”


在XV6中，这里的结构极其简单。每一个目录包含了directory entries，每一条entry都有固定的格式,每个entry总共是16个字节。：
- inode number: 前2个字节包含了目录中文件或者子目录的inode编号，
- 接下来的14个字节包含了文件或者子目录名。


## Q: 假设我们要查找路径名“/y/x”，我们该怎么做呢？
1. ：从 root inode 开始
从路径名我们知道，应该从root inode开始查找。
通常 `root inode` 会有固定的 inode 编号，在XV6中，这个编号是1。

我们该如何根据编号找到root inode呢？
用 iget(ROOTDEV, 1) 得到 root inode 
从前一节我们可以知道，inode从block 32开始，如果是 inode 1，那么必然在 block 32中的64到128字节的位置。所以文件系统可以直接读到root inode的内容。

1. 如何在 root 里找 "y"？
2.1 先读取 root inode 的 data blocks
对于路径名查找程序，接下来就是扫描 root inode 包含的所有 data blocks，以找到“y”。
该怎么找到root inode所有对应的block呢？
读取 ip->addrs[0..NDIRECT-1]， 如果有 indirect block，再读 indirect block
根据前一节的内容就是读取所有的direct block number和indirect block number。

2.2 在这些 block 里逐条扫描 struct dirent 匹配 name
```c
for each block:
  read block
  for each dirent in block:
    if (de.inum != 0 && de.name == "y"):
        bingo

```
结果可能是找到了，也可能是没有找到。

3. 找到 "y" 以后发生了什么？
如果找到了，那么目录y也会有一个inode编号，假设是251，
- 得到 inum = 251
- 用 iget(ROOTDEV, 251) 得到 inode 251
- inode 251 是目录（T_DIR）

4. 对 inode 251 重复同样的流程，找 "x"
我们可以继续从inode 251查找，
- 读 inode 251 的 data blocks
- 扫描 dirent
- 匹配 name "x"
- 得到 inode of x
最后将其作为路径名查找的结果返回。


目录 inode 里没有这些信息：
- ❌ “子目录 y 的位置”
- ❌ “文件 x 的指针”
- ❌ 树状结构

目录 inode 里只有这些信息：
- type = T_DIR
- size
- addrs[]（direct + indirect block numbers）
inode 根本不知道“y”这个名字。


那“y”这个名字在哪里？
在 目录的数据块里。 每一个数据块是 `dirent` 

block data:
+----------------+
| inum | "y\0"   |
| inum | "x\0"   |
| inum | "foo"   |
|  0   | unused  |
+----------------+


Q: 为什么不能“直接匹配 name”？为什么必须先通过 direct / indirect block 找到目录内容，才能匹配 name？
“既然是按 name 查找，为什么还要读所有 block？”

在 XV6 中，： name 根本不在 inode 里，也不在某个索引结构里。name 只是目录文件内容的一部分，不存在任何“按名索引”的结构。 
XV6 没有：
- B+ tree
- hash table
- name index

只有：一个平铺在磁盘上的 struct dirent[] 数组

所以：想找 "y" → 你只能 线性扫描所有目录数据块


dirlookup 正是在做这件事
```c
for(off = 0; off < dp->size; off += sizeof(de)){
  readi(dp, ..., &de, off, sizeof(de));
  if(de.inum == 0) continue;
  if(namecmp(name, de.name) == 0)
    return iget(dp->dev, de.inum);
}

```

这正是：
- off += sizeof(de) → 线性扫描
- readi() → 从目录 inode 的 block 中读
- namecmp() → name 匹配

XV6 的目录查找不是“查名字”，而是“读文件 + 扫描记录”。
真实 Unix 后来引入了 hash / tree



## dirlookup
The function `dirlookup` (kernel/fs.c:530) searches a directory for an entry with the given name.
- If it finds one, it returns a pointer to the corresponding `inode`, unlocked, and sets `*poff` to the byte offset of the entry within the directory, in case the caller wishes to edit it. 
  If dirlookup finds an entry with the right name, it updates `*poff` and returns an **unlocked inode** obtained via `iget`.


Q: What lock is currently held?
```c
ilock(ip);                 // lock current directory
next = dirlookup(ip, name, 0);
iunlockput(ip);            // release current directory
ip = next;                 // move forward
```

Dirlookup is the reason that `iget` returns **unlocked inodes**. 
The caller has locked dp, so if the lookup was for ., an alias for the current directory, attempting to lock the inode before returning would try to re-lock dp and deadlock. (There are more complicated deadlock scenarios involving multiple processes and .., an alias for the parent directory; . is not the only problem.) 
The caller can unlock dp and then lock ip, ensuring that it only holds one lock at a time.


```c
// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.

// So `dirlookup(dp, "x", ...)` literally means:
// “Scan the directory file dp, entry by entry, until you find one whose name is "x", then return the inode for that entry.”
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");
  
  // walking directory entries
  // dp->size = directory size in bytes
  // Each entry is sizeof(struct dirent)
  // So this walks entry 0, entry 1, entry 2…
  for(off = 0; off < dp->size; off += sizeof(de)){
    // readi reads from an inode (directory is just a file)
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    // Skipping empty directory slots
    if(de.inum == 0)
      continue;
    
    // Name comparison
    // If names match: We’ve found the path element , Now we must return the inode it refers to
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      // So poff tells the caller: “The directory entry lives at byte offset off.”
      if(poff)
        *poff = off;
      inum = de.inum;
      // iget:
      // Finds or allocates an entry in icache
      // Increments ref
      // Returns an unlocked inode
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

```


# Pathname layer 

provides hierarchical path names like /usr/rtm/xv6/fs.c, and resolves them with recursive lookup. 

# File descriptor Layer 

abstracts many Unix resources (e.g., pipes, devices, files, etc.) using the file system interface, simplifying the lives of application programmers.
