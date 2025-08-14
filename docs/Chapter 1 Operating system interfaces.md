

# OS instruction

## The job of an operating system

The job of an operating system is to share a computer among multiple programs and to provide a more useful set of services than the hardware alone supports.
- An operating system **manages and abstracts the low-level hardware**
- An operating system **shares the hardware** among multiple programs so that they run (or appear to run) at the same time. Finally, 
- operating systems provide **controlled ways for programs to interact**, so that they can share data or work together.

## Operating system interfaces

An operating system provides services to user programs through an interface. 

The collection of system calls that a kernel provides is the interface that user programs see.


### Unix operating system
 Ken Thompson and Dennis Ritchie’s Unix operating system
- Unix provides a **narrow interface** whose mechanisms combine well, offering a surprising degree of generality. 
- This interface has been so successful that modern operating systems—BSD, Linux, macOS, Solaris, and even, to a lesser extent, Microsoft Windows—have Unix-like interfaces.


### Kernel Space - System Call - User Space
![image](../images/Figure%201.1-A%20kernel%20and%20two%20user%20processes.png)

Kernel: a special program that provides services to running programs.

Process: Each running program, called a process, has memory containing instructions, data, and a stack.
  - The instructions implement the program’s computation. 
  - The data are the variables on which the computation acts. 
  - The stack organizes the program’s procedure calls.

System Call: When a process needs to invoke **a kernel service**, it invokes a system call, one of the calls in the operating system’s interface.

  - The system call enters the kernel; the kernel performs the service and returns. Thus a process alternates between executing in user space and kernel space.

  - The kernel uses the **hardware protection mechanisms** provided by a CPU to ensure that each process executing in **user space** can access only its own memory.

  - The kernel executes with the hardware privileges required to implement these protections; user programs execute without those privileges. When a user program invokes a system call, the hardware raises the privilege level and starts executing a pre-arranged function in the kernel.




# 1.1 Processes and memory

## Process:

Each running program, called a process, has memory containing instructions, data, and a stack. 

- The instructions implement the program’s computation. 
- The data are the variables on which the computation acts. 
- The stack organizes the program’s procedure calls.
A given computer typically has many processes but only a single kernel.

An xv6 process consists of user-space memory (instructions, data, and stack) and per-process
state private to the kernel.


### fork

A process may create a new process using the fork system call. 

- Fork gives the new process exactly the same memory contents (**both instructions and data**) as the calling process. 
- Fork returns in **both the original and new processes**.
- In the original process, fork returns the new process’s PID. 
- In the new process, fork returns zero. 
- The original and new processes are often called the parent and child.

```C
/**
* The wait system call returns the PID of an exited (or killed) child of the current process and copies the exit status of the child to the address passed to wait; 
 - if none of the caller’s children has exited, wait waits for one to do so. 
 - If the caller has no children, wait immediately returns -1.
 - If the parent doesn’t care about the exit status of a child, it can pass a 0 address to wait.

* The exit system call causes the calling process to stop executing and to release resources such as memory and open files. 
* Exit takes an integer status argument, conventionally 0 to indicate success and 1 to indicate failure.
*/


int pid = fork();
if(pid>0){
    printf("parent : child=%d\n", pid);
    pid = wait((int *) 0); //After the child exits, the parent’s wait returns
    printf("child=%d is done\n", pid);
}else if(pid==0){
    printf("child : exiting");
    exit(0);
}else{
    printf("fork error\n");
}
```

### exec

- The exec system call replaces the calling process’s memory with **a new memory image** loaded from a file stored in the file system. 
    The file must have a particular format, which specifies
    - which part of the file holds instructions
    - which part is data
    - at which instruction to start
    - etc

- When exec succeeds, it does not return to the calling program; 
- instead, the instructions loaded from the file start executing at the entry point declared in **the ELF header**.


```C
/**
 * Exec takes two arguments: the name of the file containing the executable and an array of string arguments.
*/
char  *argv[3];
argv[0] ="echo";
argv[1] ="hello world";
argv[2] =0;
exec("/bin/echo", argv);
prtintf("exec failed\n");
```


# 1.2 I/O and File descriptors

## File descriptors
A file descriptor is a small integer representing a kernel-managed object that a process may read from or write to.

Internally, the xv6 kernel uses the file descriptor as an index into a per-process table, so that every process has a private space of file descriptors starting at zero. 

By convention, a process reads from 
- file descriptor 0 (standard input)- 
- writes output to file descriptor 1 (standard output)
- and writes error messages to file descriptor 2 (standard error)

## read, write, open, close system calls

- int open(char *file, int flags) Open a file; flags indicate read/write; returns an fd (file descriptor).
- int write(int fd, char *buf, int n) Write n bytes from buf to file descriptor fd; returns n.
- int read(int fd, char *buf, int n) Read n bytes into buf; returns number read; or 0 if end of file.
- int close(int fd) Release open file fd.


The read and write system calls read bytes from and write bytes to open files named by file descriptors. 

###  read(fd, buf, n)
The call read(fd, buf, n) reads at most n bytes from the file descriptor fd, copies them into buf, and returns the number of bytes read. Each file escriptor that refers to a file has an offset associated with it. 
Read reads data from the current file offset and then advances that offset by the number of bytes read: a subsequent read will return the bytes following the ones returned by the first read. 
When there are no more bytes to read, read returns zero to indicate the end of the file.

### write(fd, buf, n)
The call write(fd, buf, n) writes n bytes from buf to the file descriptor fd and returns the number of bytes written. Fewer than n bytes are written only when an error occurs. Like read,write writes data at the current file offset and then advances that offset by the number of bytes
written: each write picks up where the previous one left off.

```bash
# cat1.c
#include <stdio.h>  // for printf() and fprintf()
#include <unistd.h> // for read() and write() system calls
#include <stdlib.h> //for exit()

char buf[1024];
int n;
int main(void)
{

    for (;;)
    {
        n = read(0, buf, sizeof buf);
        if (n == 0)
            break;
        if (n < 0)
        {
            // fprintf(2, "read error\n");
            // This fix resolves the type mismatch warning by using the proper FILE* pointer instead of the raw file descriptor integer.
            fprintf(stderr, "read error\n");
            exit(1);
        }
        if (write(1, buf, n) != n)
        {
            // fprintf(2, "write error\n");
            fprintf(stderr, "write error\n");
            exit(1);
        }
    }
    exit(0);
}

```


### close
The close system call releases a file descriptor, making it free for reuse by a future open, pipe, or dup system call (see below). A newly allocated file descriptor is always the lowest numbered unused descriptor of the current process.


File descriptors and fork interact to make I/O redirection easy to implement. Fork copies the parent’s file descriptor table along with its memory, so that the child starts with exactly the same open files as the parent. The system call exec replaces the calling process’s memory but preserves its file table. This behavior allows the shell to implement I/O redirection by forking, reopening chosen file descriptors in the child, and then calling exec to run the new program.


### dup

The dup system call duplicates an existing file descriptor, returning a new one that refers to the same underlying I/O object. Both file descriptors share an offset, just as the file descriptors duplicated by fork do.

# 1.3 Pipes

A pipe is a small kernel buffer exposed to processes as a pair of file descriptors, one for reading
and one for writing. Writing data to one end of the pipe makes that data available for reading from
the other end of the pipe. Pipes provide a way for processes to communicate.

# 1.4 File system

## File system

1. data files: which contain uninterpreted byte arrays
2. directories: which contain named references to data files and other directories.
3. device

```C
// kernel/stat.h
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};


```

### 1. data files
-  A file’s name is distinct from the file itself; 
-  inode: the same underlying file, called an inode
-  links: the same underlying file can have multiple names, called links. 
-  Each link consists of an entry in a directory; the entry contains a file name and a reference to an inode. 
-  An inode holds metadata about a file, including its type (file or  directory or device), its length, the location of the file’s content on disk, and the number of links to  a file.


### 2. device
mknod creates a special file that refers to a device. 
Associated with a device file are the major and minor device numbers (the two arguments to mknod), which uniquely identify a kernel device.


### 3. directory

A directory is just a file containing an array of struct dirent:
- name is just the bare filename (e.g., "etc", "README", "file1").
- No \0 terminator if the name exactly fills 14 chars.
- Absolutely no / stored in the entry — / is only used when userspace code constructs a path string.
- / is a path separator in Unix-like systems — it’s not part of a name.
- Keeping it out of struct dirent means: Names are consistent whether file or directory.

```C
struct dirent {
  ushort inum;
  char name[DIRSIZ]; // filename, up to 14 chars, no '/'
};

```

If name is shorter than 14 chars:
- xv6 pads the remaining bytes with \0 (null characters) when writing it.
- This makes it fixed-size (easier for disk read/write).


#### Example:

If you ls /:
- Path / (root) contains entries: ".", "..", "README", "bin", etc.
- None of these stored as "bin/" on disk — "bin/" is just a printing convention in ls to show it's a directory.

## System Calls

### open & mknod
```C
// mkdir creates a new directory
mkdir("/dir");

// open with the O_CREATE flag creates a new data file, 
fd = open("/dir/file", O_CREATE|O_WRONLY);
close(fd);

//  mknod creates a new device file.
mknod("/console", 1, 1);
```

### fstat

The fstat system call retrieves information from the inode that a file descriptor refers to. 
It fills in a struct stat, defined in stat.h (kernel/stat.h) as:

```C
#include "kernel/stat.h"

struct stat st;
if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

```
### link & unlink

The link system call creates another file system name referring to the same inode as an existing
file. 
```C
// This fragment creates a new file named both a and b.
// Reading from or writing to a is the same as reading from or writing to b. 
//  Each inode is identified by a unique inode number. 
// After the code sequence above, it is possible to determine that a and b refer to the same underlying contents by inspecting the result of fstat: both will return the same inode number (ino), and the nlink count will be set to 2.
open("a", O_CREATE|O_WRONLY);
link("a", "b");

//leaves the inode and file content accessible as b.
unlink("a");

```

The unlink system call removes a name from the file system. The file’s inode and the disk
space holding its content are only freed when the file’s link count is zero and no file descriptors
refer to it.

## file utilities
Unix provides file utilities callable from the shell as user-level programs, 
for example 
- mkdir
- ln
- rm


One exception is cd, which is built into the shell


# 1.5 Real world

## Unix
- Unix’s combination of “standard” file descriptors, pipes, and convenient shell syntax for operations on them was a major advance in writing general-purpose reusable programs.
- The idea sparked a culture of “software tools” that was responsible for much of Unix’s power and popularity, 
-  the shell was the first so-called “scripting language.” 
- The Unix system call interface persists today in systems like BSD, Linux, and macOS.

### POSIX(Portable Operating System Interface )
The Unix system call interface has been standardized through the Portable Operating System Interface (POSIX) standard


### Resources are files
Unix unified access to multiple types of resources (files, directories, and devices) with a single
set of file-name and file-descriptor interfaces.

## Xv6
Xv6 is not POSIX compliant: it is missing many system calls (including basic ones such as lseek), and many of the system calls it does provide differ from the standard. Our main goals for xv6 are simplicity and clarity while providing a simple UNIX-like system-call interface.

Xv6 does not provide a notion of users or of protecting one user from another; in Unix terms,
all xv6 processes run as root.

## Operating System
Any operating system must 
- multiplex processes onto the underlying hardware, 
- isolate processes from each other, 
- provide mechanisms for controlled inter-process communication.

After studying xv6, you should be able to look at other, more complex operating systems and see the concepts underlying xv6 in those systems as well.
