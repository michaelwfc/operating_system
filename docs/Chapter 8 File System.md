
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

```


```

### The disk layer 
reads and writes blocks on an virtio hard drive. 

### The buffer cache layer 

caches disk blocks and synchronizes access to them, making sure that only one kernel process at a time can modify the data stored in any particular block. 

这些cache可以避免频繁的读写磁盘。这里我们将磁盘中的数据保存在了内存中。

### The logging layer 

allows higher layers to wrap updates to several blocks in a transaction, and ensures that the blocks are updated **atomically** in the face of crashes (i.e., all of them are updated or none). 


在logging层之上，XV6有inode cache，这主要是为了同步（synchronization），我们稍后会介绍。inode通常小于一个disk block，所以多个inode通常会打包存储在一个disk block中。为了向单个inode提供同步操作，XV6维护了 inode cache。

### The inode layer 

provides individual files, each represented as an inode with a unique i-number and some blocks holding the file’s data. 


### The directory layer 

implements each directory as a special kind of inode whose content is a sequence of **directory entries**, each of which contains a file’s name and i-number. 

### The pathname layer 

provides hierarchical path names like /usr/rtm/xv6/fs.c, and resolves them with recursive lookup. 

### The file descriptor layer 

abstracts many Unix resources (e.g., pipes, devices, files, etc.) using the file system interface, simplifying the lives of application programmers.

## Storage Device

SSD通常是0.1到1毫秒的访问时间，而HDD通常是在10毫秒量级完成读写一个disk block。

sectors和blocks。

- sector通常是磁盘驱动可以读写的最小单元，它过去通常是512字节。

- block通常是操作系统或者文件系统视角的数据。它由文件系统定义，在XV6中它是 1024字节。所以XV6中一个block对应两个sector。通常来说一个block对应了一个或者多个sector。

有的时候，人们也将磁盘上的sector称为block。所以这里的术语也不是很精确。


## Disk layout

```

| boot | supper block |   log   | inode | bitmap | data  |
  0           1         2 ~    32      45      46    

```
从文件系统的角度来看磁盘还是很直观的。因为对于磁盘就是读写block或者sector，我们可以将磁盘看作是一个巨大的block的数组，数组从0开始，一直增长到磁盘的最后。

而文件系统的工作就是将所有的数据结构以一种能够在重启之后重新构建文件系统的方式，存放在磁盘上。虽然有不同的方式，但是XV6使用了一种非常简单，但是还挺常见的布局结构。

通常来说：
- block0:  要么没有用，要么被用作boot sector来启动操作系统。

- block1: 通常被称为 **super block**，它描述了文件系统。它可能包含磁盘上有多少个block共同构成了文件系统这样的信息。我们之后会看到XV6在里面会存更多的信息，你可以通过block1构造出大部分的文件系统信息。

- log:   在XV6中，log从block2开始，到block32结束。实际上log的大小可能不同，这里在super block中会定义log就是30个block。

- inode: 接下来在block32到block45之间，XV6存储了 inode 。inode 区域（inode blocks）。 
  我之前说过多个inode会打包存在一个block中。
  一个 inode = 64 字节
  一个 block = 1024 字节
 所以： 一个 block 能装 1024 / 64 = 16 个 inode

 inode blocks: block 32 ~ block 45 = 45 - 32 + 1 = 14 个 block
 14 × 16 = 224 个 inode
 这个文件系统一共支持 224 个 inode

- bitmap block: 
  之后是 bitmap block，这是我们构建文件系统的默认方法，它只占据一个block。它记录了数据block是否空闲。
  
- data block: 之后就全是数据 block 了，数据block存储了文件的内容和目录的内容。

通常来说，bitmap block，inode blocks 和 log blocks被统称为 **metadata block**。 它们虽然不存储实际的数据，但是它们存储了能帮助文件系统完成工作的元数据。




# The inode layer 

最重要的可能就是inode，这是代表一个文件的对象，并且它不依赖于文件名。
- inode number
实际上，inode是通过自身的编号来进行区分的，这里的编号就是个整数。所以文件系统内部通过一个数字，而不是通过文件路径名引用inode。
- link count
同时，基于之前的讨论，inode必须有一个link count来跟踪指向这个inode的文件名的数量。
一个文件（inode）只能在link count为0的时候被删除。实际的过程可能会更加复杂，实际中还有一个openfd count，也就是当前打开了文件的文件描述符计数。一个文件只能在这两个计数器都为0的时候才能被删除。


## inode Structure

```c
// kernel/fs.h
// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

```
这是一个64字节的数据结构。
- type : 通常来说它有一个type字段，表明inode是文件,目录, 设备文件。
  A type of zero indicates that an ondisk inode is free.
- nlink字段，也就是link计数器，用来跟踪究竟有多少文件名指向了当前的inode。
- size字段，表明了文件数据有多少个字节。
- direct block number
  磁盘被切成一个个 block（在 xv6 里是 1024 字节）。
  inode 里保存的是：“这些 block 在磁盘上的编号”。

不同文件系统中的表达方式可能不一样，不过在XV6中接下来是一些block的编号，例如编号0，编号1，等等。

XV6的inode中总共有12个block编号。这12个block编号指向了构成文件的前12个block。
举个例子，如果文件只有2个字节，那么只会有一个block编号0，它包含的数字是磁盘上文件前2个字节的block的位置。

这意味着： 小文件（≤ 12 KB）只需要 inode 就能找到全部数据,  不用额外磁盘访问，快得很

- indirect block number
  inode 的第 13 个指针：不再指向数据， 而是指向 一个“索引 block”
它对应了磁盘上一个block，这个block包含了256个block number，这256个block number包含了文件的数据。

所以inode中block number 0到block number 11都是direct block number，而block number 12保存的indirect block number指向了另一个block。


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


## 目录（directory）

文件系统的酷炫特性就是层次化的命名空间（hierarchical namespace），你可以在文件系统中保存对用户友好的文件名。大部分Unix文件系统有趣的点在于，一个目录本质上是一个文件加上一些文件系统能够理解的结构。

在XV6中，这里的结构极其简单。每一个目录包含了directory entries，每一条entry都有固定的格式,每个entry总共是16个字节。：
- inode number: 前2个字节包含了目录中文件或者子目录的inode编号，
- 接下来的14个字节包含了文件或者子目录名。


假设我们要查找路径名“/y/x”，我们该怎么做呢？

从路径名我们知道，应该从root inode开始查找。通常root inode会有固定的inode编号，在XV6中，这个编号是1。我们该如何根据编号找到root inode呢？
从前一节我们可以知道，inode从block 32开始，如果是inode1，那么必然在block 32中的64到128字节的位置。所以文件系统可以直接读到root inode的内容。

对于路径名查找程序，接下来就是扫描root inode包含的所有block，以找到“y”。该怎么找到root inode所有对应的block呢？根据前一节的内容就是读取所有的direct block number和indirect block number。
结果可能是找到了，也可能是没有找到。如果找到了，那么目录y也会有一个inode编号，假设是251，

我们可以继续从inode 251查找，先读取inode 251的内容，之后再扫描inode所有对应的block，找到“x”并得到文件x对应的inode编号，最后将其作为路径名查找的结果返回。

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


## Debug 


<!-- XV6创建inode代码展示 -->
```bash
# Inside the xv6 shell:
make clean && qemu-gdb
echo "hi" > x.csv

# In gdb:
gdb-multiarch kernel/kernel

b sys_open
b create
b ialloc

b iget
b dirlink

b balloc
b bwrite
# In xv6, almost all disk writes eventually call:
# bwrite(struct buf *b)

(gdb) c
```

### 1. sys_open — the entry point
Everything starts here: `sys_open`
```bash
sys_open
 └─ begin_op()          ← logging starts
 └─ create()            ← because O_CREATE
 └─ end_op()            ← commit log
```
接下来我们通过查看XV6中的代码，更进一步的了解文件系统。因为我们前面已经分配了inode，我们先来看一下这是如何发生的。sysfile.c 中包含了所有与文件系统相关的函数，分配inode 发生在 `sys_open` 函数中，这个函数会负责创建文件。

### 2. create() — birth of file x

This is where ialloc, iget, bwrite first appear.

create函数中首先会解析路径名并找到最后一个目录，之后会查看文件是否存在，如果存在的话会返回错误。之后就会调用 `ialloc`（inode allocate），这个函数会为文件x分配inode。 `ialloc` 函数位于 `fs.c` 文件中。
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



以上就是第一次写磁盘涉及到的函数调用。这里有个有趣的问题，如果有多个进程同时调用create函数会发生什么？对于一个多核的计算机，进程可能并行运行，两个进程可能同时会调用到 ialloc 函数，然后进而调用bread（block read）函数。所以必须要有一些机制确保这两个进程不会互相影响。

让我们看一下位于bio.c的buffer cache代码。首先看一下bread函数

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

// bread函数首先会调用bget函数，bget会为我们从buffer cache中找到block的缓存。让我们看一下bget函数


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }


```

是的，我们这里看一下block 33的cache是否存在，如果存在的话，将block对象的引用计数（refcnt）加1，之后再释放bcache锁，因为现在我们已经完成了对于cache的检查并找到了block cache。之后，代码会尝试获取block cache的锁。

所以，如果有多个进程同时调用bget的话，其中一个可以获取bcache的锁并扫描buffer cache。此时，其他进程是没有办法修改buffer cache的（注，因为bacche的锁被占住了）。之后，进程会查找block number是否在cache中，如果在的话将block cache的引用计数加1，表明当前进程对block cache有引用，之后再释放bcache的锁。如果有第二个进程也想扫描buffer cache，那么这时它就可以获取bcache的锁。假设第二个进程也要获取block 33的cache，那么它也会对相应的block cache的引用计数加1。最后这两个进程都会尝试对block 33的block cache调用acquiresleep函数。

acquiresleep是另一种锁，我们称之为sleep lock，本质上来说它获取block 33 cache的锁。其中一个进程获取锁之后函数返回。在ialloc函数中会扫描block 33中是否有一个空闲的inode。而另一个进程会在acquiresleep中等待第一个进程释放锁。

## Sleep Lock


# Buffer cache layer

The buffer cache has two jobs: 
- (1) synchronize access to disk blocks to ensure that only one copy
of a block is in memory and that only one kernel thread at a time uses that copy;
- (2) cache popular blocks so that they don’t need to be re-read from the slow disk. 
  The code is in bio.c.
The main interface exported by the buffer cache consists of `bread` and `bwrite`; 
- `bread`:  obtains a buf containing a copy of a block which can be read or modified in memory, and
- `bwrite`:  writes a modified buffer to the appropriate block on the disk. 
  
  A kernel thread must release a buffer by calling `brelse` when it is done with it. The buffer cache uses a per-buffer sleep-lock to ensure that only one thread at a time uses each buffer (and thus each disk block); bread returns a locked buffer, and brelse releases the lock.

Let’s return to the buffer cache. The buffer cache has a fixed number of buffers to hold disk blocks, which means that if the file system asks for a block that is not already in the cache, the buffer cache must recycle a buffer currently holding some other block. The buffer cache recycles the least recently used buffer for the new block. The assumption is that the least recently used buffer is the one least likely to be used again soon.





## block cache

以上就是对于 block cache 代码的介绍。这里有几件事情需要注意：

首先在内存中，对于一个block只能有一份缓存。这是block cache必须维护的特性。

其次，这里使用了与之前的spinlock略微不同的sleep lock。与spinlock不同的是，可以在I/O操作的过程中持有sleep lock。

第三，它采用了LRU作为cache替换策略。

第四，它有两层锁。第一层锁用来保护buffer cache的内部数据；第二层锁也就是sleep lock用来保护单个block的cache。



# Crash recovery

我们将会看到很多文件系统的操作都包含了多个步骤，如果我们在多个步骤的错误位置crash或者电力故障了，存储在磁盘上的文件系统可能会是一种不一致的状态，之后可能会发生一些坏的事情。

我们今天会研究对于这类特定问题的解决方法，也就是logging。这是一个最初来自于数据库世界的很流行的解决方案，现在很多文件系统都在使用logging。


## Exampels

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


## logging layer
What the logging layer is (conceptually)

The logging layer (also called journaling) is a small subsystem that:
- records intent before modifying real filesystem structures
- replays or discards changes after a crash

It does not log file data by default (in xv6).
It logs metadata blocks.

Think of it as:

“Before I change reality, I write down what I’m going to change.”

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
- The header block 
  contains an array of sector numbers, one for each of the logged blocks, and the count of log blocks. 
  The count in the header block on disk is either zero, indicating that there is no transaction in the log, 
  or nonzero, indicating that the log contains a complete committed transaction with the indicated number of logged blocks. 
  
  Xv6 writes the header block when a transaction commits, but not before, and sets the count to zero after copying the logged blocks to the file system. 
  Thus a crash midway through a transaction will result in a count of zero in the log’s header block; 
  a crash after a commit will result in a non-zero count.

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


## Debug

```bash

# These help you see the logging layer in action.
b log_write
b commit
```