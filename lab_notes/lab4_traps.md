This lab explores how system calls are implemented using traps. You will first do a warm-up exercises with stacks and then you will implement an example of user-level trap handling.

Before you start coding, read Chapter 4 of the xv6 book, and related source files:
- kernel/trampoline.S: the assembly involved in changing from user space to kernel space and back
- kernel/trap.c: code handling all interrupts

To start the lab, switch to the trap branch:
```
  $ git fetch
  $ git checkout traps
  $ make clean
```

# RISC-V assembly (easy)
It will be important to understand a bit of RISC-V assembly, which you were exposed to in 6.004. There is a file `user/call.c` in your xv6 repo. make fs.img compiles it and also produces a readable assembly version of the program in `user/call.asm`.

Read the code in `call.asm` for the functions g, f, and main. 

The instruction manual for RISC-V is on [the reference page](https://pdos.csail.mit.edu/6.828/2020/reference.html). 

Here are some questions that you should answer (store the answers in a file answers-traps.txt):

## Reference
- [MIT 6.S081学习笔记（第四章）](https://luyoung0001.github.io/2023/11/24/MIT%206.S081%E5%AD%A6%E4%B9%A0%E7%AC%94%E8%AE%B0%EF%BC%88%E7%AC%AC%E5%9B%9B%E7%AB%A0%EF%BC%89/)
- 

## GDB setting
### .gdbinit_user 
```bash
# gdbinit file for debugging user/_call
# $ gdb-multiarch -x .gdbinit_user 
set confirm off
set architecture riscv:rv64
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes
set disassemble-next-line on
set print pretty on 
set print array on


# If you want to debug the user program call, load its symbols so GDB knows where main() is:
# Now GDB understands your user/call.c functions (main, f, g, etc.).
symbol-file user/_call

target remote :26000

b user/call.c:main
```

### start gdb

```bash
$ make clean & make qemu-gdb
(qemu)$ call

# after make qemu, call will generate call.asm
$code user/call.asm 
# or you can genereate call.asm by using objdump
riscv64-linux-gnu-objdump -S  user/_call.o > user/call.asm
void main(void) {
  1c:	1141                	addi	sp,sp,-16
  1e:	e406                	sd	ra,8(sp)
  20:	e022                	sd	s0,0(sp)
  22:	0800                	addi	s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  24:	4635                	li	a2,13
  26:	45b1                	li	a1,12
  28:	00000517          	auipc	a0,0x0
  2c:	7d850513          	addi	a0,a0,2008 # 800 <malloc+0xea>
  30:	00000097          	auipc	ra,0x0
  34:	628080e7          	jalr	1576(ra) # 658 <printf>
  exit(0);
  38:	4501                	li	a0,0
  3a:	00000097          	auipc	ra,0x0
  3e:	27e080e7          	jalr	638(ra) # 2b8 <exit>

$ gdb-multiarch -x .gdbinit_user
(gdb) c 



```
1. Q: Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?

a0~a7, 事实上，当函数的参数超过 8 个时，就会保存到栈空间；否则，就会依次保存到寄存器中。从这里也可以看出，当可以使用寄存器的时候，我们不会使用内存，我们只在不得不使用内存的场景才使用它。从 main的汇编代码可以看到，13 被存到a2中。

```bash
(gdb) where
#0  main () at user/call.c:16
(gdb) next

(gdb) p $pc
$1 = (void (*)()) 0x24 <main+8>

#  in user/call.asm
  # 24:	4635                	li	a2,13
  # 26:	45b1                	li	a1,12
  # 28:	00000517          	auipc	a0,0x0
(si) si
(gdb) si
   0x0000000000000024 <main+8>: 35 46   li      a2,13
=> 0x0000000000000026 <main+10>:        b1 45   li      a1,12
   0x0000000000000028 <main+12>:        17 15 00 00     auipc   a0,0x1
   0x000000000000002c <main+16>:        13 05 05 84     addi    a0,a0,-1984 # 0x868
   0x0000000000000030 <main+20>:        97 00 00 00     auipc   ra,0x0
   0x0000000000000034 <main+24>:        e7 80 00 69     jalr    1680(ra) # 0x6c0 <printf>

(gdb) p $a2
$1 = 13

(gdb) p $a1
$2 = 12
(gdb) p $pc
$3 = (void (*)()) 0x28 <main+12>


```


2. Q: Where is the call to function f in the assembly code for main? Where is the call to g? (Hint: the compiler may inline functions.)

Answer: 

Where is the call to f?
There isn’t one. The compiler inlined f() and then folded it entirely into a constant value (12) inside main.

Where is the call to g?
Also gone. The compiler inlined g() into f(), and since f() was also inlined, g() never appears in the final main at all.

main函数里面没有f 的汇编代码，这是因为它被优化了。函数 f 就是使传入的参数加 3后返回。考虑到编译器会进行内联优化，这就意味着一些显而易见的，编译时可以计算的数据会在编译时得出结果，而不是进行函数调用。其它同理。


```bash
int g(int x) {
   0:	1141                	addi	sp,sp,-16
   2:	e422                	sd	s0,8(sp)
   4:	0800                	addi	s0,sp,16
  return x+3;
}
   6:	250d                	addiw	a0,a0,3
   8:	6422                	ld	s0,8(sp)
   a:	0141                	addi	sp,sp,16
   c:	8082                	ret

000000000000000e <f>:

int f(int x) {
   e:	1141                	addi	sp,sp,-16
  10:	e422                	sd	s0,8(sp)
  12:	0800                	addi	s0,sp,16
  return g(x);
}
  14:	250d                	addiw	a0,a0,3
  16:	6422                	ld	s0,8(sp)
  18:	0141                	addi	sp,sp,16
  1a:	8082                	ret


# So both g() and f() are compiled to exactly the same code!
# That means the compiler inlined g() inside f() — it replaced the call to g(x) with x + 3.

# No jal (jump and link) instruction inside f. So, no call to g remains at runtime.


000000000000001c <main>:
  24: 4635                	li	a2,13
  26: 45b1                	li	a1,12 
  28: 00000517            	auipc	a0,0x0
  2c: 7d850513            	addi	a0,a0,2008 # 800 <malloc+0xea>
  30: 00000097            	auipc	ra,0x0
  34: 628080e7            	jalr	1576(ra) # 658 <printf>
  exit(0);
  38:	4501                li	a0,0
  3a:	00000097          	auipc	ra,0x0
  3e:	27e080e7          	jalr	638(ra) # 2b8 <exit>
# Notice something big: there’s no jal instruction calling either f or g — the only call is to printf at address 0x658.

li	a1,12 
# That means the compiler computed f(8)+1 as a constant 12 at compile time:
f(8) = g(8) = 8 + 3 = 11
11 + 1 = 12
# So the compiler did constant folding — it realized the result never changes and precomputed it.


```

3. Q: At what address is the function printf located?

A: 0x658

```bash
  34:	628080e7          	jalr	1576(ra) # 658 <printf>

# in user/call.asm
0000000000000658 <printf>:
void
printf(const char *fmt, ...)
{
 658:	711d                	addi	sp,sp,-96
 65a:	ec06                	sd	ra,24(sp)
 65c:	e822                	sd	s0,16(sp)
 65e:	1000                	addi	s0,sp,32
 660:	e40c                	sd	a1,8(s0)
 662:	e810                	sd	a2,16(s0)
 664:	ec14                	sd	a3,24(s0)
 666:	f018                	sd	a4,32(s0)
 668:	f41c                	sd	a5,40(s0)
 66a:	03043823          	sd	a6,48(s0)
 66e:	03143c23          	sd	a7,56(s0)

# The RISC-V jalr instruction performs two things atomically:
# 1. It saves the address of the next instruction (PC+4) into the destination register (ra here, x1).
# - The jalr at 0x34 is 4 bytes long, so the next instruction after it would be at 0x38:
# - Set ra = 0x38 ← the return address (the instruction after the call)

# 2.It jumps to the computed target address.
# - Jump to address 0x658 (the entry of printf)


(gdb) where
#0  0x0000000000000030 in main () at user/call.c:16
(gdb) p/x $pc
$9 = 0x30

(gdb) p/x $ra
$10 = 0x30

(gdb) p/x $ra + 1576
$14 = 0x658
```

4. What value is in the register ra just after the jalr to printf in main?

Just after the jalr to printf, the register ra holds 0x38.
That’s the return address, i.e., where control will resume in main after printf finishes.

```bash
  28:	00000517          	auipc	a0,0x0
  2c:	7d850513          	addi	a0,a0,2008 # 800 <malloc+0xea>
  30:	00000097          	auipc	ra,0x0
  34:	628080e7          	jalr	1576(ra) # 658 <printf>

# 使用 auipc ra,0x0 将当前程序计数器 pc 的值存入 ra 中；

(gdb) p/x $ra
$8 = 0x30
(gdb) p/x $ra+1576
$9 = 0x658
(gdb) si
printf (fmt=fmt@entry=0x800 "%d %d\n") at user/printf.c:108
=> 0x0000000000000658 <printf+0>:       1d 71   addi    sp,sp,-96
   0x000000000000065a <printf+2>:       06 ec   sd      ra,24(sp)
   0x000000000000065c <printf+4>:       22 e8   sd      s0,16(sp)
   0x000000000000065e <printf+6>:       00 10   addi    s0,sp,32
   0x0000000000000660 <printf+8>:       0c e4   sd      a1,8(s0)
   0x0000000000000662 <printf+10>:      10 e8   sd      a2,16(s0)
   0x0000000000000664 <printf+12>:      14 ec   sd      a3,24(s0)
   0x0000000000000666 <printf+14>:      18 f0   sd      a4,32(s0)
   0x0000000000000668 <printf+16>:      1c f4   sd      a5,40(s0)
   0x000000000000066a <printf+18>:      23 38 04 03     sd      a6,48(s0)
   0x000000000000066e <printf+22>:      23 3c 14 03     sd      a7,56(s0)

(gdb) p $ra
$15 = (void (*)()) 0x38 <main+28>


# What’s the “next instruction” in main?
# The jalr at 0x34 is 4 bytes long, so the next instruction after it would be at 0x38:
  38:	4501                li	a0,0

# When the CPU executes:
jalr 1576(ra)
# it will:
# - Set ra = 0x38 ← the return address (the instruction after the call)
# - Jump to address 0x658 (the entry of printf)



```

5. Run the following code.

	unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, &i);
      
Q1: What is the output? Here's an ASCII table that maps bytes to characters.

```bash
(gdb) p 57616
$5 = 57616
(gdb) p/x 57616
$6 = 0xe110
# 57616 转换为 16 进制为 e110，所以格式化描述符 %x 打印出了它的 16 进制值。所以会打印出：He110

(gdb) p/x 0x00646c72
# In hex bytes, that’s:
#  how many bytes does the compiler use to store the value 0x00646c72? 4 bytes
00 64 6c 72

# What’s stored in memory for i
(gdb) p/x i
$9 = 0x646c72

(gdb) p (char *)&i
$10 = 0x2fcc "rld"
# &i → the address of the variable i.
# (char*)&i → reinterpret that address as a pointer to char instead of a pointer to an unsigned int.
# That means:
# “Treat the memory of i as a sequence of characters starting from its first byte.”
# When GDB prints a char*, it assumes it’s pointing to a C-string — that is, a sequence of characters ending with a null byte ('\0').
# So GDB looks at memory starting from that address, reads bytes, and prints characters until it finds 0x00.

# On a little-endian system, i = 0x00646c72 is stored in memory as 72 6c 64 00, so this prints "rld".


# orce GDB to print the value as a 4-character array
# That’s a nice compact form — tells GDB to treat the 4 bytes at &i as a 4-element char[].
(gdb) p *(char[4]*) &i
$11 =   "rld"

# Examine 4 bytes of memory as characters
# If you want to see each byte as a character, even if there’s a '\0':
(gdb) x/4cb &i
0x2fcc: 114 'r' 108 'l' 100 'd' 0 '\000'
# Breakdown:
# x = examine memory
# /4 = show 4 units
# c = show as character
# b = one-byte units
# That’s exactly how the 32-bit value is laid out in memory.

# Examine as hexadecimal bytes (for verification)
(gdb) x/4xb &i
0x2fcc: 0x72    0x6c    0x64    0x00
# Which corresponds to 'r', 'l', 'd', '\0'.

# In little-endian order, memory stores the least significant byte first:
| Address | Byte (hex) | ASCII | Meaning           |
| ------- | ---------- | ----- | ----------------- |
| &i      | 72         | 'r'   |                   |
| &i+1    | 6c         | 'l'   |                   |
| &i+2    | 64         | 'd'   |                   |
| &i+3    | 00         | '\0'  | string terminator |

# So the bytes in memory look like:
'r' 'l' 'd' '\0'


```
Q2: The output depends on that fact that the RISC-V is little-endian. If the RISC-V were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?

Here's a description of [little- and big-endian](https://www.webopedia.com/definitions/big-endian/) and a more whimsical description.

```bash
# In big-endian systems, the most significant byte is stored first.
# So for the same 0x00646c72, memory layout would be:
| Address | Byte (hex) | ASCII |
| ------- | ---------- | ----- |
| &i      | 00         | '\0'  |
| &i+1    | 64         | 'd'   |
| &i+2    | 6c         | 'l'   |
| &i+3    | 72         | 'r'   |

# So &i now points to the '\0' character — that means %s would print nothing, because the string starts with the terminator.
# To get the same "rld" in memory on a big-endian system, you need to reverse the byte order when initializing i:
i= 0x 72 6c 64 00;

# Because in big-endian, this will be laid out as bytes:
| Byte | ASCII |
| ---- | ----- |
| 72   | 'r'   |
| 6c   | 'l'   |
| 64   | 'd'   |
| 00   | '\0'  |

# Do we need to change 57616?
# No. The %x part doesn’t depend on memory layout — integers are printed as numbers, not byte arrays. Endianness only matters when you reinterpret memory, as with &i and %s.

| Question | Answer                                                  |
| -------- | ------------------------------------------------------- |
| Q1       | Output is `He110 World`                                 |
| Q2       | For big-endian: `i = 0x726c6400`; keep `57616` the same |


```

6. Q: In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
	printf("x=%d y=%d", 3);

  
```bash
Q6
x=3 y=1
```
A: 函数的参数是通过寄存器a1, a2 等来传递。如果 prinf 少传递一个参数，那么其仍会从一个确定的寄存器中读取其想要的参数值，但是我们并没有给出这个确定的参数并将其存储在寄存器中，所以函数将从此寄存器中获取到一个随机的不确定的值作为其参数。故而此例中，y=后面的值我们不能够确定，它是一个垃圾值。

# Backtrace (moderate)

For debugging it is often useful to have a backtrace: a list of the function calls on the stack above the point at which the error occurred.

Implement a backtrace() function in `kernel/printf.c`. Insert a call to this function in `sys_sleep`, and then run `bttest`, which calls sys_sleep. Your output should be as follows:
```bash
backtrace:
0x0000000080002cda
0x0000000080002bb6
0x0000000080002898
```
After bttest exit qemu. In your terminal: the addresses may be slightly different but if you run `addr2line -e kernel/kernel` (or `riscv64-unknown-elf-addr2line -e kernel/kernel`) and cut-and-paste the above addresses as follows:
```bash
$ addr2line -e kernel/kernel
0x0000000080002de2
0x0000000080002f4a
0x0000000080002bfc
Ctrl-D

# You should see something like this:
kernel/sysproc.c:74
kernel/syscall.c:224
kernel/trap.c:85


The compiler puts in each stack frame a frame pointer(saved frame pointer) that holds the address of the caller's frame pointer. Your backtrace should use these frame pointers to walk up the stack and print the saved return address in each stack frame.

---------------------
Previous stack frame

---------------------
current frame pointer
saved return address  
saved frame pointer   -> previous frame pointer
local variables
.....
current stack pointer
-----------------
```

## Some hints:
- Add the prototype for backtrace to `kernel/defs.h` so that you can invoke backtrace in sys_sleep.
- The GCC compiler stores the frame pointer of the currently executing function in the register s0. Add the following function to `kernel/riscv.h`:
  
```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}
```

and call this function in `backtrace` to read the current frame pointer. This function uses in-line assembly to read s0.

- These lecture notes have a picture of the layout of stack frames. Note that the `return address` lives at a fixed offset (-8) from the frame pointer of a stackframe, and that the `saved frame pointer` lives at fixed offset (-16) from the frame pointer.

- Xv6 allocates one page for each stack in the xv6 kernel at PAGE-aligned address. You can compute the top and bottom address of the stack page by using `PGROUNDDOWN(fp)` and `PGROUNDUP(fp)` (see kernel/riscv.h. These number are helpful for backtrace to terminate its loop.

Once your backtrace is working, call it from panic in kernel/printf.c so that you see the kernel's backtrace when it panics.


## Solution


## Dubugging

```bash
Stack
                   .
                   .
      +->          .
      |   +-----------------+   |
      |   | return address  |   |
      |   |   previous fp ------+
      |   | saved registers |
      |   | local variables |
      |   |       ...       | <-+
      |   +-----------------+   |
      |   | return address  |   |
      +------ previous fp   |   |
          | saved registers |   |
          | local variables |   |
      +-> |       ...       |   |
      |   +-----------------+   |
      |   | return address  |   |
      |   |   previous fp ------+
      |   | saved registers |
      |   | local variables |
      |   |       ...       | <-+
      |   +-----------------+   |
      |   | return address  |   |
      +------ previous fp   |   |
          | saved registers |   |
          | local variables |   |
  $fp --> |       ...       |   |
          +-----------------+   |
          | return address  |   |
          |   previous fp ------+
          | saved registers |
  $sp --> | local variables |
          +-----------------+

(gdb) b sleep
# Backtrace (moderate)

(gdb) bt
#0  backtrace () at kernel/printf.c:140
#1  0x0000000080002226 in sleep (chan=chan@entry=0x80017798 <bcache+24>,
    lk=lk@entry=0x800250a8 <disk+8360>) at kernel/proc.c:562
#2  0x00000000800060d4 in virtio_disk_rw (b=b@entry=0x80017798 <bcache+24>,
    write=write@entry=0) at kernel/virtio_disk.c:242
#3  0x0000000080002f96 in bread (dev=dev@entry=1, blockno=blockno@entry=1)
    at kernel/bio.c:99
#4  0x000000008000344e in readsb (sb=0x8001fe40 <sb>, dev=1) at kernel/fs.c:43
#5  fsinit (dev=dev@entry=1) at kernel/fs.c:43
#6  0x0000000080001a7c in forkret () at kernel/proc.c:550
#7  0x0000000080001a38 in myproc () at kernel/proc.c:73
Backtrace stopped: frame did not save the P

(gdb) i frame
# Stack level 0, frame at 0x3fffffdee0:
#  pc = 0x800007b4 in backtrace (kernel/printf.c:140); saved pc = 0x80002226
#  called by frame at 0x3fffffdf10
#  source language c.
#  Arglist at 0x3fffffdee0, args:
#  Locals at 0x3fffffdee0, Previous frame's sp is 0x3fffffdee0
#  Saved registers:
#   ra at 0x3fffffded8, fp at 0x3fffffded0, pc at 0x3fffffded8 
#   Could not fetch register "ustatus"; remote failure reply 'E14'

"""
This tells us:
- Stack level 0，表明这是调用栈的最底层
- Current frame (current stack frame): 0x3fffffdee0
- pc，当前的程序计数器，Current program counter (PC): 0x800007b4 — the instruction currently executing (inside backtrace).
- Current function: backtrace()
- saved pc，表明当前函数要返回的位置
- Caller’s frame (previous stack frame): 0x3fffffdf10
- source language c，表明这是C代码
- Arglist at，表明参数的起始地址。当前的参数都在寄存器中，args: 为空。 
- Locals at，表明本地变量的起始地址。

Saved registers:
  ra at 0x3fffffded8, fp at 0x3fffffded0, pc at 0x3fffffded8 

- Saved return address (ra): 0x80002226  — the address to jump back to after backtrace returns at frame 0x3fffffded8.
So backtrace was called by sleep(), and when it returns, execution should resume at address 0x80002226 inside sleep.
"""


(gdb) p $ra
$19 = (void (*)()) 0x80002226 <sleep+26>

# confirming that the saved ra on the stack matches the live $ra register
(gdb) x/gx 0x3fffffded8
0x3fffffded8:   0x0000000080002226

"""
Saved return address (ra): 0x80002226 
- 0x80002226 - The address value of ra
- the address to jump back to after backtrace returns.
So backtrace was called by sleep(), and when it returns, execution should resume at address 0x80002226 inside sleep.
- At memory address 0x3fffffded8, there’s an 8-byte saved copy of the caller’s ra.

Why p $ra shows 0x80002226 ？

When backtrace() was called from sleep(), the CPU did this:
jal ra, backtrace

That instruction saved the address of the next instruction after the call into ra.
So ra = 0x80002226 is exactly where backtrace() will return to when it executes ret.

And since backtrace()’s prologue saved ra into the stack at 0x3fffffded8, both match:

Saved ra on stack: 0x3fffffded8 → value 0x80002226
Current ra register: 0x80002226

|--------------------------|  ← 0x3fffffdf10  (caller frame)
|  caller’s locals         |
|--------------------------|  new frame
|  current frame pointer   |  ← 0x3fffffdee0 = s0 = sp + 16,  fp — top of the frame (sp + 16)
|  saved ra (0x80002226)   |  ← 0x3fffffded8 
|  saved fp (0x3fffffdf10) |  ← 0x3fffffded0 , saved old s0 = 0x3fffffdf10
|  local variables         |
|--------------------------|
↑
current sp                 | <- 0x3fffffded0 , bottom of the frame
"""

# saved frame pointer
(gdb) x/gx 0x3fffffded0
0x3fffffded0:   0x0000003fffffdf10

# current frame pointer
(gdb) p/x $s0
$30 = 0x3fffffdee0

# current frame pointer
# $fp = the current frame’s frame pointer
(gdb) p/x $fp
$29 = 0x3fffffdee0 
# in this frame, why fp is 0x3fffffdee0 not 0x3fffffdf10? 


"""
- the saved frame pointer — belonging to the caller (sleep) — is stored on the stack at address sp + 0 = 0x3fffffded0 + 0 0x3fffffded0.
At 0x3fffffded0, there’s the saved frame pointer 0x0000003fffffdf10 (caller frame).
The CPU (or compiler-generated prologue) saves the caller’s frame pointer so that it can be restored later when returning.

Each frame points backward to the caller’s frame, forming a linked chain of frame pointers — that’s exactly how GDB unwinds the call stack for bt.


How the prologue works on RISC-V
A typical function prologue in xv6 (compiled for RISC-V) looks like this:
addi sp, sp, -16      # allocate 16 bytes for stack frame
sd   ra, 8(sp)        # save return address
sd   s0, 0(sp)        # save old frame pointer
addi s0, sp, 16       # set new frame pointer = top of this frame (points above locals)


"""




(gdb) x *$s0
Attempt to dereference a generic pointer.

"""
What happens when backtrace() returns
ld ra, -8(s0)
ld s0, -16(s0)
ret

it restores ra from the stack (value 0x80002226) and jumps there, resuming execution in sleep.
"""
(gdb) p $sp
$35 = (void *) 0x3fffffded0



```

## Test
```
$ make clean && make qemu
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ bttest
0x0000000080002cc4
0x0000000080002b9e
0x0000000080002888

After bttest exit qemu. In your terminal: the addresses may be slightly different but if you run `addr2line -e kernel/kernel` (or `riscv64-unknown-elf-addr2line -e kernel/kernel`) and cut-and-paste the above addresses as follows:
```
```bash
$ addr2line -e kernel/kernel
0x0000000080002cc4
0x0000000080002b9e
0x0000000080002888
/mnt/e/projects/operating_system/xv6-labs-2020/kernel/sysproc.c:63
/mnt/e/projects/operating_system/xv6-labs-2020/kernel/syscall.c:151
/mnt/e/projects/operating_system/xv6-labs-2020/kernel/trap.c:76
```


# Alarm (hard)
In this exercise you'll add a feature to xv6 that periodically alerts a process as it uses CPU time. This might be useful for compute-bound processes that want to limit how much CPU time they chew up, or for processes that want to compute but also want to take some periodic action. More generally, you'll be implementing a primitive form of `user-level interrupt/fault handlers`; you could use something similar to handle page faults in the application, for example. Your solution is correct if it passes `alarmtest` and `usertests`.

You should add a new `sigalarm`(interval, handler) system call. If an application calls `sigalarm(n, fn)`, then after every n "ticks" of CPU time that the program consumes, the kernel should cause application function fn to be called. When fn returns, the application should resume where it left off. 

A tick is a fairly arbitrary unit of time in xv6, determined by how often a hardware timer generates interrupts. If an application calls `sigalarm(0, 0)`, the kernel should stop generating periodic alarm calls.

You'll find a file `user/alarmtest.c` in your xv6 repository. Add it to the `Makefile`. It won't compile correctly until you've added `sigalarm` and `sigreturn` system calls (see below).

`alarmtest` calls `sigalarm(2, periodic)` in test0 to ask the kernel to force a call to periodic() every 2 ticks, and then spins for a while. You can see the assembly code for alarmtest in `user/alarmtest.asm`, which may be handy for debugging. 

## test
Your solution is correct when alarmtest produces output like this and `usertests` also runs correctly:

```bash
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
$ usertests
...
ALL TESTS PASSED
$
```

When you're done, your solution will be only a few lines of code, but it may be tricky to get it right. We'll test your code with the version of` alarmtest.c` in the original repository. You can modify `alarmtest.c` to help you debug, but make sure the original alarmtest says that all the tests pass.


## test0: invoke handler

Get started by modifying the kernel to jump to the alarm handler in user space, which will cause test0 to print "alarm!". Don't worry yet what happens after the "alarm!" output; it's OK for now if your program crashes after printing "alarm!". 

### Here are some hints:
1. You'll need to modify the Makefile to cause `alarmtest.c` to be compiled as an xv6 user program.
2. The right declarations to put in `user/user.h` are:
    `int sigalarm(int ticks, void (*handler)())`;
    `int sigreturn(void)`;
3. Update `user/usys.pl` (which generates `user/usys.S`), `kernel/syscall.h`, and `kernel/syscall.c` to allow alarmtest to invoke the `sigalarm` and `sigreturn` system calls.
4. For now, your `sys_sigreturn` should just return zero.
5. Your `sys_sigalarm()` should store the alarm interval and the pointer to the handler function in new fields in the `proc structure` (in kernel/proc.h).
You'll need to keep track of how many ticks have passed since the last call (or are left until the next call) to a process's alarm handler; you'll need a new field in `struct proc` for this too. You can initialize `proc` fields in `allocproc()` in proc.c.
6. Every tick, the hardware clock forces an interrupt, which is handled in `usertrap()` in `kernel/trap.c`.
7. You only want to manipulate a process's alarm ticks if there's a timer interrupt; you want something like
    `if(which_dev == 2) ...`
8. Only invoke the alarm function if the process has a timer outstanding. Note that the address of the user's alarm function might be 0 (e.g., in `user/alarmtest.asm`, periodic is at address 0).
9. You'll need to modify `usertrap()` so that when a process's alarm interval expires, the user process executes the handler function. When a trap on the RISC-V returns to user space, what determines the instruction address at which user-space code resumes execution?
10. It will be easier to look at traps with gdb if you tell qemu to use only one CPU, which you can do by running
   ` make CPUS=1 qemu-gdb`
You've succeeded if alarmtest prints "alarm!".

### Timer Interrpter workflow 

1. KERNEL: initialize `proc` fields
   allocproc()
3. USERMODE: alartest: call test0()
4. KERNEL:  register timer interrupt handler
   call sigalarm(2, periodic)
   syscall trap: scause=8
   - uservec
   - usetrap
     - scause=8 -> syscall -> sys_sigalarm(): register timer interrupt:
   - usertrapret
   - userret: returns to user space
6. KERNEL:  track the ticks and executes the timer interrupt handler function 
   RISC-V machine timer says “ding” → trap → usertrap()
   当 timer 报“ding！”时，它会触发一个 S-mode interrupt。 
   硬件做三件事：
    1. 把当前用户 PC 保存到 sepc
    2. 把中断原因写到 scause
    3. 跳到 stvec 指定的地址（xv6 设置成 uservec）
    无论是什么 trap（syscall/中断/异常），这一过程都一样。

   usevec()
   usertrap() : devintr() ==2 （Timer interrput）
      - increment ticks
      - check alram counters
      - if intrval hit:
         - save trapframe
         - set trapframe.epc = periodic
  
   yield() : 把 CPU 让出来，让 scheduler 再次调度这个进程, 强制这次 trap 立即结束
     - sched()  
     - swtch(): 保存当前进程的上下文（寄存器等）,切换到 CPU 上的 scheduler context
     - scheduler context
     - scheduler picks p again
     - swtch back to p (in scheduler): scheduler 现在会找一个 RUNNABLE 的进程运行
     - swtch(&cpu->scheduler_context, &p->context): 未来某个时刻：调度器决定切回这个进程
     - 继续执行 yield() 后面
     - 返回到 usertrap() 
   - usertrapret: 
     - 恢复 trapframe 的寄存器 （包括你刚刚改的 epc）恢复到 CPU 真正的寄存器里
   - userret: 执行 sret, returns to user space
     - 跳到 sepc 中存的地址（也就是 periodic）
   - pc = trapframe.epc = periodic(): 真正开始执行 periodic()
  
  USERMODE: suddenly enters periodic()
    → call sigreturn()
  KERNEL: restore trapframe
    ↓ return to user
  USERMODE: continue where interrupted
   

### Debugging test0

#### 1. allocproc : initialize proc fields

```bash
# in .gdbinit_kernel
add-symbol-file user/_alarmtest
b user/alarmtest.c:main
b sys_sigalarm
# b *0x3ffffff000    # Common xv6 trampoline virtual address

```
#### 2. call sigalarm(2, periodic) :register timer interrupt handler

```bash
(qemu) $ alarmtest

(gdb) where
#0  main (argc=1, argv=0x2fe0) at user/alarmtest.c:23
#1  0x00000000000000fe in test0 () at user/alarmtest.c:50
Backtrace stopped: Cannot access memory at address 0x3f98

# 现在 GDB 在 ecall 前停下（用户态）
(gdb) where
#0  sigalarm () at user/usys.S:136
#1  0x00000000000000f4 in test0 () at user/alarmtest.c:48
#2  0x00000000000003b4 in main (argc=<optimized out>, argv=<optimized out>) at user/alarmtest.c:24
#3  0x00000000000000fe in test0 () at user/alarmtest.c:49
Backtrace stopped: Cannot access memory at address 0x3f98


(gdb) x/6i $pc
=> 0x70e <sigalarm+2>:  ecall
   0x712 <sigalarm+6>:  ret
   0x714 <sigreturn>:   li      a7,28
   0x716 <sigreturn+2>: ecall
   0x71a <sigreturn+6>: ret
   0x71c <putc>:        addi    sp,sp,-32

(gdb) p $pc
$7 = (void (*)()) 0x70e <sigalarm+2>


(gdb) p $a0
$8 = 2
(gdb) p $a1
$9 = 0
(gdb) x $a1
   0x0 <periodic>:      addi    sp,sp,-16


# stvec point to trampoline page,which call ecall, which will jump to uservec
# which is set by usertrapret
(gdb) p/x $stvec
$11 = 0x3ffffff000

# must set a breakpoint at  0x3ffffff000 and set riscv use-compressed-breakpoints yes in .gdbinit
# set a breakpoint at the trampoline page address
(gdb) b *$stvec
Breakpoint 2 at 0x3ffffff000
# (gdb) b *0x3ffffff000    # Common xv6 trampoline virtual address


(gdb) si
Breakpoint 2, 0x0000003ffffff000 in ?? ()
=> 0x0000003ffffff000:  73 15 05 14     csrrw   a0,sscratch,a0
```

#####  uservec (kernel/trampoline.S)

On trap entry (`uservec` in trampoline.S):
1. Save user registers into the process’s `trapframe`.
2. Switches to kernel stack so we can run C code
3. Switch `satp` from the user’s page table to the kernel page table.
4. Jump into the kernal trap handler (`usertrap()` in trap.c).
   
```bash
# swap a0 and sscratch
# so that a0 is TRAPFRAME

(gdb) p/x $sscratch
$13 = 0x3fffffe000
(gdb) p/x $a0
$14 = 0x2

(gdb) p/x $ra
$15 = 0xf4
(gdb) p/x $sp
$16 = 0x2f90

# # load the address of usertrap(), p->trapframe->kernel_trap
# ld t0, 16(a0)
(gdb) p/x $t0
$20 = 0x80002836
(gdb) x $t0
0x80002836 <usertrap>:  Cannot access memory at address 0x80002836
(gdb) x *$t0
Cannot access memory at address 0x80002836

(gdb) si
0x0000003ffffff08a in ?? ()
=> 0x0000003ffffff08a:  73 00 00 12     sfence.vma
0x0000003ffffff08e in ?? ()
=> 0x0000003ffffff08e:  82 82   jr      t0
usertrap () at kernel/trap.c:38
=> 0x0000000080002836 <usertrap+0>:     01 11   addi    sp,sp,-32
   0x0000000080002838 <usertrap+2>:     06 ec   sd      ra,24(sp)
   0x000000008000283a <usertrap+4>:     22 e8   sd      s0,16(sp)
   0x000000008000283c <usertrap+6>:     26 e4   sd      s1,8(sp)
   0x000000008000283e <usertrap+8>:     4a e0   sd      s2,0(sp)
   0x0000000080002840 <usertrap+10>:    00 10   addi    s0,sp,32

```
##### usertrap (kernel/trap.c)
```bash

(gdb) p/x $sepc
$25 = 0x70e
(gdb) p/x p->trapframe->epc
$26 = 0x70e

(gdb) x/5i p->trapframe->epc
   0x70e <sigalarm+2>:  Cannot access memory at address 0x70e


(gdb) x/5i p->trapframe->epc +4
   0x712 <sigalarm+6>:  Cannot access memory at address 0x712


(gdb) p/x *(struct trapframe*)p->trapframe


(gdb) p/x which_dev
$29 = 0x1


(gdb) p/x *(struct proc*)p
$28 = {
  lock = {
    locked = 0x0,
    name = 0x800081e0,
    cpu = 0x0
  },
  state = 0x3,
  parent = 0x80011ee0,
  chan = 0x0,
  killed = 0x0,
  xstate = 0x0,
  pid = 0x3,
  kstack = 0x3fffff9000,
  sz = 0x3000,
  pagetable = 0x87f48000,
  trapframe = 0x87f64000,
  context = {
    ra = 0x800020aa,
    sp = 0x3fffff9a60,
    s0 = 0x3fffff9a90,
    s1 = 0x80012058,
    s2 = 0x80011950,
    s3 = 0x1,
    s4 = 0x80025000,
    s5 = 0x80023000,
    s6 = 0x2000,
    s7 = 0x80023000,
    s8 = 0x8,
    s9 = 0x53c,
    s10 = 0x0,
    s11 = 0x400
  },
  ofile =     {0x80021e68,
    0x80021e68,
    0x80021e68,
    0x0 <repeats 13 times>},
  cwd = 0x80020278,
  name =     {0x61,
    0x6c,
    0x61,
    0x72,
    0x6d,
    0x74,
    0x65,
    0x73,
    0x74,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0},
  alarm_interval = 0x0,
  alarm_ticks = 0x0,
  alarm_handler = 0x0
}


```
##### Issue:  
1. devintr() =1 in trap.c/usertrap
2. alarm_interval = 0x0 

##### sys_sigalarm()

```bash
(gdb) where
#0  sys_sigalarm () at kernel/sysproc.c:132
#1  0x0000000080002bc8 in syscall () at kernel/syscall.c:160
#2  0x0000000080002892 in usertrap () at kernel/trap.c:67
#3  0x000000000000010c in test0 () at user/alarmtest.c:50
Backtrace stopped: previous frame inner to this frame (corrupt stack?)
```



##### usertrapret()
##### userret()


#### 3. track the ticks and executes the timer interrupt handler function

##### usertrap()
```bash
# set a breakpoint at trap when timer interrupt
(gdb)b kernel/trap.c:88

(gdb) c

(gdb) where
#0  usertrap () at kernel/trap.c:88
#1  0x000000000000013e in test0 () at user/alarmtest.c:53
Backtrace stopped: previous frame inner to this frame (corrupt stack?)


```

##### yield()

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

(gdb) b scheduler

(gdb) where
#0  yield () at kernel/proc.c:568
#1  0x00000000800028f6 in usertrap () at kernel/trap.c:123
#2  0x000000000000013e in test0 () at user/alarmtest.c:53
Backtrace stopped: previous frame inner to this frame (corrupt stack?)

```

##### shed()

```c
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;

  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}




(gdb) p p->name
$6 =   "alarmtest\000\000\000\000\000\000"

//  swtch(&p->context, &mycpu()->context); 
(gdb) b scheduler
Breakpoint 5 at 0x80001f6e: file kernel/proc.c, line 463.
```

##### usertrapret()




## test1/test2(): resume interrupted code

Chances are that alarmtest crashes in test0 or test1 after it prints "alarm!", or that alarmtest (eventually) prints "test1 failed", or that alarmtest exits without printing "test1 passed". To fix this, you must ensure that, when the alarm handler is done, control returns to the instruction at which the user program was originally interrupted by the timer interrupt. You must ensure that the register contents are restored to the values they held at the time of the interrupt, so that the user program can continue undisturbed after the alarm. Finally, you should "re-arm" the alarm counter after each time it goes off, so that the handler is called periodically.

As a starting point, we've made a design decision for you: user alarm handlers are required to call the sigreturn system call when they have finished. Have a look at periodic in `alarmtest.c` for an example. This means that you can add code to `usertrap` and `sys_sigreturn` that cooperate to cause the user process to resume properly after it has handled the alarm.

### Some hints:

1. Your solution will require you to save and restore registers---what registers do you need to save and restore to resume the interrupted code correctly? (Hint: it will be many).
2. Have `usertrap` save enough state in `struct proc` when the timer goes off that sigreturn can correctly return to the interrupted user code.
3. Prevent re-entrant calls to the handler----if a handler hasn't returned yet, the kernel shouldn't call it again. test2 tests this.
Once you pass test0, test1, and test2 run usertests to make sure you didn't break any other parts of the kernel.


Once you pass test0, test1, and test2 run usertests to make sure you didn't break any other parts of the kernel.