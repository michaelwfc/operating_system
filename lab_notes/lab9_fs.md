# Large files (moderate)
In this assignment you'll increase the maximum size of an xv6 file. Currently xv6 files are limited to 268 blocks, or `268*BSIZE` bytes (BSIZE is 1024 in xv6). This limit comes from the fact that an xv6 inode contains 12 "direct" block numbers and one "singly-indirect" block number, which refers to a block that holds up to 256 more block numbers, for a total of 12+256=268 blocks.

The bigfile command creates the longest file it can, and reports that size:

```bash
$ bigfile
..
wrote 268 blocks
bigfile: file is too small
$
```

The test fails because bigfile expects to be able to create a file with **65803 blocks**, but unmodified xv6 limits files to 268 blocks.
You'll change the xv6 file system code to support **a "doubly-indirect" block in each inode**, containing 256 addresses of singly-indirect blocks, each of which can contain up to 256 addresses of data blocks. The result will be that a file will be able to consist of up to 65803 blocks, or **256*256+256+11** blocks (11 instead of 12, because we will sacrifice one of the direct block numbers for the double-indirect block).

## Preliminaries
The `mkfs` program creates the xv6 file system disk image and determines how many total blocks the file system has; this size is controlled by `FSSIZE` in `kernel/param.h`. You'll see that `FSSIZE` in the repository for this lab is set to 200,000 blocks. You should see the following output from` mkfs/mkfs` in the make output:
```bash
# nmeta 70 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 25) blocks 199930 total 200000

mkfs/mkfs fs.img README  user/_cat user/_echo user/_forktest user/_grep user/_init user/_kill user/_ln user/_ls user/_mkdir user/_rm user/_sh user/_stressfs user/_usertests user/_grind user/_wc user/_zombie  user/_bigfile
nmeta 70 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 25) blocks 199930 total 200000
balloc: first 643 blocks have been allocated
balloc: write bitmap block at sector 45
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 1 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

init: starting sh
$

```
This line describes the file system that mkfs/mkfs built: it has 70 meta-data blocks (blocks used to describe the file system) and 199,930 data blocks, 
If at any point during the lab you find yourself having to rebuild the file system from scratch, you can run make clean which forces make to rebuild `fs.img`.

## What to Look At
The format of an on-disk inode is defined by `struct dinode` in fs.h. You're particularly interested in `NDIRECT`, `NINDIRECT`, `MAXFILE`, and the `addrs[]` element of struct dinode. Look at Figure 8.3 in the xv6 text for a diagram of the standard xv6 inode.

The code that finds a file's data on disk is in `bmap()` in fs.c. Have a look at it and make sure you understand what it's doing. `bmap()` is called both when reading and writing a file. 
When writing, `bmap()` allocates new blocks as needed to hold file content, as well as allocating an indirect block if needed to hold block addresses.

bmap() deals with two kinds of block numbers. 
- The `bn argument` is a "logical block number" -- a block number within the file, relative to the start of the file. 
- The block numbers in `ip->addrs[]`, and the argument to bread(), are disk block numbers. 

You can view bmap() as mapping a file's logical block numbers into disk block numbers.

## Your Job
Modify `bmap()` so that it implements a doubly-indirect block, in addition to direct blocks and a singly-indirect block. 
You'll have to have only 11 direct blocks, rather than 12, to make room for your new doubly-indirect block; you're not allowed to change the size of an on-disk inode. 
- The first 11 elements of ip->addrs[] should be direct blocks; 
- the 12th should be a singly-indirect block (just like the current one); 
- the 13th should be your new doubly-indirect block. 

##  Test
You are done with this exercise when bigfile writes 65803 blocks and usertests runs successfully:

```bash
$ bigfile
..................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................
wrote 65803 blocks
done; ok
$ usertests
...
ALL TESTS PASSED
$ 
```
bigfile will take at least a minute and a half to run.


## Hints:

- Make sure you understand `bmap()`. Write out a diagram of the relationships between `ip->addrs[]`, the indirect block, the doubly-indirect block and the singly-indirect blocks it points to, and data blocks. Make sure you understand why adding a doubly-indirect block increases the maximum file size by 256*256 blocks (really -1, since you have to decrease the number of direct blocks by one).
- Think about how you'll index the doubly-indirect block, and the indirect blocks it points to, with the logical block number.
- If you change the definition of `NDIRECT`, you'll probably have to change the declaration of `addrs[]` in struct inode in `file.h`. Make sure that `struct inode` and `struct dinode` have the same number of elements in their addrs[] arrays.
- If you change the definition of `NDIRECT`, make sure to create a new `fs.img`, since `mkfs` uses `NDIRECT` to build the file system.
- If your file system gets into a bad state, perhaps by crashing, delete fs.img (do this from Unix, not xv6). make will build a new clean file system image for you.
- Don't forget to `brelse()` each block that you `bread()`.
- You should allocate indirect blocks and doubly-indirect blocks only as needed, like the original bmap().
- Make sure `itrunc` frees all blocks of a file, including double-indirect blocks.

## Solution


### 我们要改成的结构（目标）

```bash
ip->addrs[0..10]   : 11 个 direct blocks
ip->addrs[11]      : 1 个 singly-indirect block
ip->addrs[12]      : 1 个 doubly-indirect block

# doubly-indirect 的含义
addrs[12]  --->  [ indirect block A ]
                  |-> addr[0] -> singly-indirect block -> data blocks
                  |-> addr[1] -> singly-indirect block -> data blocks
                  ...
                  |-> addr[255]

```

## Debug
```bash
$ make clean && make qemu
mkfs/mkfs fs.img README  user/_cat user/_echo user/_forktest user/_grep user/_init user/_kill user/_ln user/_ls user/_mkdir user/_rm user/_sh user/_stressfs user/_usertests user/_grind user/_wc user/_zombie  user/_bigfile
nmeta 70 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 25) blocks 199930 total 200000
balloc: first 643 blocks have been allocated
balloc: write bitmap block at sector 45
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 1 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

init: starting sh
$ bigfile
..................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................
wrote 65803 blocks
bigfile done; ok
```


# Symbolic links (moderate)
In this exercise you will add symbolic links to xv6. Symbolic links (or soft links) refer to a linked file by pathname; when a symbolic link is opened, the kernel follows the link to the referred file. Symbolic links resembles hard links, but hard links are restricted to pointing to file on the same disk, while symbolic links can cross disk devices. Although xv6 doesn't support multiple devices, implementing this system call is a good exercise to understand how pathname lookup works.

## Your job
You will implement the `symlink(char *target, char *path)` system call, which creates a new symbolic link at path that refers to file named by target. For further information, see the man page `symlink`. 

## Test
To test, add `symlinktest` to the Makefile and run it. Your solution is complete when the tests produce the following output (including usertests succeeding).
```bash
$ symlinktest
Start: test symlinks
test symlinks: ok
Start: test concurrent symlinks
test concurrent symlinks: ok
$ usertests
...
ALL TESTS PASSED
$ 
```

## Hints:
1. First, create a new system call number for `symlink`, add an entry to `user/usys.pl`, `user/user.h`, and implement an empty `sys_symlink` in `kernel/sysfile.c`.
2. Add a new file type (`T_SYMLINK`) to `kernel/stat.h` to represent a symbolic link.
3. Add a new flag to `kernel/fcntl.h`, (`O_NOFOLLOW`), that can be used with the `open` system call. 
   Note that flags passed to `open` are combined using a bitwise `OR` operator, so your new flag should not overlap with any existing flags. This will let you compile `user/symlinktest.c` once you add it to the Makefile.
4. Implement the `symlink(target, path)` system call to create a new symbolic link at path that refers to target. 
   Note that target does not need to exist for the system call to succeed. You will need to choose somewhere to store the target path of a symbolic link, for example, in the inode's data blocks. symlink should return an integer representing success (0) or failure (-1) similar to link and unlink.
5. Modify the `open` system call to handle the case where the path refers to a symbolic link. 
   - If the file does not exist, open must fail. 
   - When a process specifies `O_NOFOLLOW` in the flags to open, open should open the `symlink` (and not follow the symbolic link).
6. If the linked file is also a symbolic link, you must recursively follow it until a non-link file is reached. If the links form a cycle, you must return an error code. You may approximate this by returning an error code if the depth of links reaches some threshold (e.g., 10).
7. Other system calls (e.g., `link` and `unlink`) must not follow symbolic links; these system calls operate on the symbolic link itself.
8. You do not have to handle symbolic links to directories for this lab.


## Big Picture

### What are symbolic links(Soft Link) vs sys_link(Hard link)?

Key difference (one sentence)
- Hard links share an inode.
- Symbolic links store a pathname and are resolved dynamically.

#### sys_link(Hard link)
A hard link is:  Another directory entry that points to the same inode

Properties:
- Same inode number
- Same data blocks
- Increments nlink
- Cannot cross devices
- Cannot refer to non-existent files

```bash
dirent -> inode -> data
# Multiple dirents can point to the same inode.
# That’s what sys_link() already does.
```

#### symbolic links(Soft Link)
A symbolic link is: A separate inode whose contents are a pathname string
Properties:
- Has its own inode
- File type: T_SYMLINK
- Data blocks store "../some/path"
- Target may not exist
- May cross devices (conceptually)
- Followed during open() unless O_NOFOLLOW
- 
```bash
dirent -> symlink inode -> "path/to/target"
# The kernel interprets the contents at open time.
```

## Solution

### What xv6 needs to change (high-level)
xv6 currently:
- Resolves pathnames → inode
- Assumes inode is final
- Has no concept of link-following

To support symlinks, we must:
- Introduce a new inode type: T_SYMLINK
- Store target path inside symlink inode
- Teach open() how to follow symlinks
- Detect cycles
- Respect O_NOFOLLOW

Everything else (link, unlink, stat) should not follow symlinks





### How to implement
1. #define T_SYMLINK 4 in kernel/stat.h
Q: How will the new file type (`T_SYMLINK`) be used?
Used by open(), stat() 


#define O_NOFOLLOW 0x800

Q: How will the new flag (`O_NOFOLLOW`) be used?


## Debug

```bash
$ symlinktest
Start: test symlinks
succeced to open a
succeced to stat b
succed to write to a
succed to stat b
succeced to get st.type of b is symlink
succed to open b
FAILURE: failed to read bytes from b
Start: test concurrent symlinks
test concurrent symlinks: ok
```


```c
  fd2 = open("/testsymlink/b", O_RDWR);
  if(fd2 < 0){
    fail("failed to open b");}
  else{
    printf("succed to open b\n");
  }

  // expect: open(b) → follow symlink → open(a) → read 'a'
  read(fd2, &c, 1);
  if (c != 'a'){
    fail("failed to read bytes from b");}
  else{
    printf("succed to read bytes from b\n");
  }
```

At the moment sys_open() returns  f->ip must be inode of /testsymlink/a (type == T_FILE)
If instead it is inode of /testsymlink/b (type == T_SYMLINK) ,then read() will read the symlink’s stored path bytes → exactly what you see.


### Why sys_read() must NOT be changed (you were right)

Symbolic links are pathname-resolution semantics, not file I/O semantics.

Linux rule (and xv6 follows it): Symlinks are resolved at open time, never at read/write time


```bash
$ symlinktest
Start: test symlinks
succeced to open a
succeced to stat b
succed to write to a
succed to stat b
succeced to get st.type of b is symlink
succed to open b
succed to read bytes from b
test symlinks: ok
Start: test concurrent symlinks
test concurrent symlinks: ok
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
test sbrkbugs: usertrap(): unexpected scause 0x000000000000000c pid=3243
            sepc=0x000000000000710a stval=0x000000000000710a
usertrap(): unexpected scause 0x000000000000000c pid=3244
            sepc=0x000000000000710a stval=0x000000000000710a
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
test kernmem: usertrap(): unexpected scause 0x000000000000000d pid=6170
            sepc=0x00000000000057e6 stval=0x0000000080000000
usertrap(): unexpected scause 0x000000000000000d pid=6171
            sepc=0x00000000000057e6 stval=0x000000008000c350
usertrap(): unexpected scause 0x000000000000000d pid=6172
            sepc=0x00000000000057e6 stval=0x00000000800186a0
usertrap(): unexpected scause 0x000000000000000d pid=6173
            sepc=0x00000000000057e6 stval=0x00000000800249f0
usertrap(): unexpected scause 0x000000000000000d pid=6174
            sepc=0x00000000000057e6 stval=0x0000000080030d40
usertrap(): unexpected scause 0x000000000000000d pid=6175
            sepc=0x00000000000057e6 stval=0x000000008003d090
usertrap(): unexpected scause 0x000000000000000d pid=6176
            sepc=0x00000000000057e6 stval=0x00000000800493e0
usertrap(): unexpected scause 0x000000000000000d pid=6177
            sepc=0x00000000000057e6 stval=0x0000000080055730
usertrap(): unexpected scause 0x000000000000000d pid=6178
            sepc=0x00000000000057e6 stval=0x0000000080061a80
usertrap(): unexpected scause 0x000000000000000d pid=6179
            sepc=0x00000000000057e6 stval=0x000000008006ddd0
usertrap(): unexpected scause 0x000000000000000d pid=6180
            sepc=0x00000000000057e6 stval=0x000000008007a120
usertrap(): unexpected scause 0x000000000000000d pid=6181
            sepc=0x00000000000057e6 stval=0x0000000080086470
usertrap(): unexpected scause 0x000000000000000d pid=6182
            sepc=0x00000000000057e6 stval=0x00000000800927c0
usertrap(): unexpected scause 0x000000000000000d pid=6183
            sepc=0x00000000000057e6 stval=0x000000008009eb10
usertrap(): unexpected scause 0x000000000000000d pid=6184
            sepc=0x00000000000057e6 stval=0x00000000800aae60
usertrap(): unexpected scause 0x000000000000000d pid=6185
            sepc=0x00000000000057e6 stval=0x00000000800b71b0
usertrap(): unexpected scause 0x000000000000000d pid=6186
            sepc=0x00000000000057e6 stval=0x00000000800c3500
usertrap(): unexpected scause 0x000000000000000d pid=6187
            sepc=0x00000000000057e6 stval=0x00000000800cf850
usertrap(): unexpected scause 0x000000000000000d pid=6188
            sepc=0x00000000000057e6 stval=0x00000000800dbba0
usertrap(): unexpected scause 0x000000000000000d pid=6189
            sepc=0x00000000000057e6 stval=0x00000000800e7ef0
usertrap(): unexpected scause 0x000000000000000d pid=6190
            sepc=0x00000000000057e6 stval=0x00000000800f4240
usertrap(): unexpected scause 0x000000000000000d pid=6191
            sepc=0x00000000000057e6 stval=0x0000000080100590
usertrap(): unexpected scause 0x000000000000000d pid=6192
            sepc=0x00000000000057e6 stval=0x000000008010c8e0
usertrap(): unexpected scause 0x000000000000000d pid=6193
            sepc=0x00000000000057e6 stval=0x0000000080118c30
usertrap(): unexpected scause 0x000000000000000d pid=6194
            sepc=0x00000000000057e6 stval=0x0000000080124f80
usertrap(): unexpected scause 0x000000000000000d pid=6195
            sepc=0x00000000000057e6 stval=0x00000000801312d0
usertrap(): unexpected scause 0x000000000000000d pid=6196
            sepc=0x00000000000057e6 stval=0x000000008013d620
usertrap(): unexpected scause 0x000000000000000d pid=6197
            sepc=0x00000000000057e6 stval=0x0000000080149970
usertrap(): unexpected scause 0x000000000000000d pid=6198
            sepc=0x00000000000057e6 stval=0x0000000080155cc0
usertrap(): unexpected scause 0x000000000000000d pid=6199
            sepc=0x00000000000057e6 stval=0x0000000080162010
usertrap(): unexpected scause 0x000000000000000d pid=6200
            sepc=0x00000000000057e6 stval=0x000000008016e360
usertrap(): unexpected scause 0x000000000000000d pid=6201
            sepc=0x00000000000057e6 stval=0x000000008017a6b0
usertrap(): unexpected scause 0x000000000000000d pid=6202
            sepc=0x00000000000057e6 stval=0x0000000080186a00
usertrap(): unexpected scause 0x000000000000000d pid=6203
            sepc=0x00000000000057e6 stval=0x0000000080192d50
usertrap(): unexpected scause 0x000000000000000d pid=6204
            sepc=0x00000000000057e6 stval=0x000000008019f0a0
usertrap(): unexpected scause 0x000000000000000d pid=6205
            sepc=0x00000000000057e6 stval=0x00000000801ab3f0
usertrap(): unexpected scause 0x000000000000000d pid=6206
            sepc=0x00000000000057e6 stval=0x00000000801b7740
usertrap(): unexpected scause 0x000000000000000d pid=6207
            sepc=0x00000000000057e6 stval=0x00000000801c3a90
usertrap(): unexpected scause 0x000000000000000d pid=6208
            sepc=0x00000000000057e6 stval=0x00000000801cfde0
usertrap(): unexpected scause 0x000000000000000d pid=6209
            sepc=0x00000000000057e6 stval=0x00000000801dc130
OK
test sbrkfail: usertrap(): unexpected scause 0x000000000000000d pid=6217
            sepc=0x0000000000005a5e stval=0x0000000000014000
OK
test sbrkarg: OK
test validatetest: OK
test stacktest: usertrap(): unexpected scause 0x000000000000000d pid=6221
            sepc=0x00000000000061a8 stval=0x0000000000011b40
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
