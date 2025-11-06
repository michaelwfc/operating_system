
# Reference

- https://mit-public-courses-cn-translatio.gitbook.io/mit6-s081/lec05-calling-conventions-and-stack-frames-risc-v


# 5.1 C程序到汇编程序的转换
ISA（Instruction Set Architecture）

C->ASM-> Binary(Object/.ofiles)


当我们说到一个RISC-V处理器时，意味着这个处理器能够理解RISC-V的指令集。所以，任何一个处理器都有一个关联的ISA（Instruction Sets Architecture），ISA就是处理器能够理解的指令集。每一条指令都有一个对应的二进制编码或者一个Opcode。当处理器在运行时，如果看见了这些编码，那么处理器就知道该做什么样的操作。上图中的处理器正好能理解RISC-V汇编语言。

所以通常来说，要让C语言能够运行在你的处理器之上。我们首先要写出C程序，之后这个C程序需要被编译成汇编语言。这个过程中有一些链接和其他的步骤，但是因为这门课不是一个编译器的课程，所以我们忽略这些步骤。之后汇编语言会被翻译成二进制文件也就是.obj或者.o文件。

汇编语言不具备C语言的组织结构，在汇编语言中你只能看到一行行的指令，比如add，mult等等。汇编语言中没有很好的控制流程，没有循环（注，但是有基于lable的跳转），虽然有函数但是与你们知道的C语言函数不太一样，汇编语言中的函数是以label的形式存在而不是真正的函数定义。汇编语言是一门非常底层的语言，许多其他语言，比如C++，都会编译成汇编语言。运行任何编译型语言之前都需要先生成汇编语言。

# 5.2 RISC-V vs x86

RISC-V和x86并没有它们第一眼看起来那么相似。RISC-V中的RISC是精简指令集（Reduced Instruction Set Computer）的意思，而x86通常被称为CISC，复杂指令集（Complex Instruction Set Computer）。这两者之间有一些关键的区别：

- 首先是指令的数量。
实际上，创造RISC-V的一个非常大的初衷就是因为Intel手册中指令数量太多了。x86-64指令介绍由3个文档组成，并且新的指令以每个月3条的速度在增加。因为x86-64是在1970年代发布的，所以我认为现在有多于15000条指令。RISC-V指令介绍由两个文档组成。在这节课中，不需要你们记住每一个RISC-V指令，但是如果你感兴趣或者你发现你不能理解某个具体的指令的话，在课程网站的参考页面有RISC-V指令的两个文档链接。这两个文档包含了RISC-V的指令集的所有信息，分别是240页和135页，相比x86的指令集文档要小得多的多。这是有关RISC-V比较好的一个方面。所以在RISC-V中，我们有更少的指令数量。

- 除此之外，RISC-V指令也更加简单。
  在x86-64中，很多指令都做了不止一件事情。这些指令中的每一条都执行了一系列复杂的操作并返回结果。但是RISC-V不会这样做，RISC-V的指令趋向于完成更简单的工作，相应的也消耗更少的CPU执行时间。这其实是设计人员的在底层设计时的取舍。并没有一些非常确定的原因说RISC比CISC更好。它们各自有各自的使用场景。

- 相比x86来说，RISC另一件有意思的事情是它是开源的。这是市场上唯一的一款开源指令集，这意味着任何人都可以为RISC-V开发主板。RISC-V是来自于UC-Berkly的一个研究项目，之后被大量的公司选中并做了支持，网上有这些公司的名单，许多大公司对于支持一个开源指令集都感兴趣。

如果查看RISC-V的文档，可以发现RISC-V的特殊之处在于：它区分了Base Integer Instruction Set和Standard Extension Instruction Set。Base Integer Instruction Set包含了所有的常用指令，比如add，mult。除此之外，处理器还可以选择性的支持Standard Extension Instruction Set。例如，一个处理器可以选择支持Standard Extension for Single-Precision Float-Point。这种模式使得RISC-V更容易支持向后兼容。 每一个RISC-V处理器可以声明支持了哪些扩展指令集，然后编译器可以根据支持的指令集来编译代码。


## 主流指令集(Instruction Set Architectures ISA)与对应操作系统

---



当然可以！下面我会列出一些**主流的指令集架构（ISA）**，以及每种指令集上常见、流行或者经典的**操作系统（OS）**，方便你全面了解它们之间的对应关系。

| 指令集架构（ISA）          | 类型       | 特点简述                  | 支持的著名操作系统                                      |
| ------------------- | -------- | --------------------- | ---------------------------------------------- |
| **x86 / x86\_64**   | CISC     | 老牌 PC 架构，性能强，耗电大      | Windows, Linux 各发行版, BSD, macOS（老款Intel）       |
| **ARM / ARM64**     | RISC     | 高能效，主导移动设备，Apple M 芯片 | Android, iOS, Linux, Windows on ARM, HarmonyOS |
| **RISC-V**          | RISC（开源） | 完全开源，适合研究、嵌入式、芯片开发    | Linux（Debian, Fedora等）, FreeBSD, Zephyr, RTEMS |
| **MIPS**            | RISC     | 曾在嵌入式中流行，现多用于教育领域     | Linux, RTEMS, QNX, BSD                         |
| **Power / PowerPC** | RISC     | IBM 推出，曾用于旧 Mac、游戏机等  | AIX（IBM）, Linux（如Debian PPC）, AmigaOS          |
| **SPARC**           | RISC     | Sun 推出，多用于服务器         | Solaris, Linux（Debian SPARC）                   |
| **Itanium (IA-64)** | VLIW     | Intel/HP 推出，失败架构      | Windows Server（旧版本）, Linux                     |
| **Z/Architecture**  | CISC     | IBM 大型机架构             | z/OS, Linux on IBM Z                           |

---

## 详细举例

### 1. **x86 / x86\_64**

* 操作系统：

  * Microsoft Windows（包括 XP, 7, 10, 11 等）
  * 各种 Linux 发行版：Ubuntu, Debian, Fedora, Arch, CentOS…
  * FreeBSD, OpenBSD, NetBSD
  * macOS（10.15 Catalina 及更早版本使用 Intel x86）
  * ReactOS（Windows 克隆）
  * MS-DOS, FreeDOS
* 典型设备：台式机、笔记本、服务器

---

### 2. **ARM / ARM64**

* 操作系统：

  * Android（绝大多数手机和平板）
  * iOS（iPhone、iPad）
  * Linux（Raspberry Pi 就是 ARM 架构）
  * Windows 10/11 on ARM
  * HarmonyOS（华为）
  * macOS（Apple M 系列芯片，M1/M2）
* 典型设备：手机、平板、树莓派、嵌入式设备、Apple M1/M2 电脑

---

### 3. **RISC-V**

* 操作系统：

  * Linux（Debian RISC-V、Fedora RISC-V、Ubuntu RISC-V）
  * FreeBSD（有移植）
  * Zephyr、RTEMS、NuttX（RTOS）
  * xv6-riscv（MIT 教学OS）
  * seL4（安全微内核）
* 典型设备：实验开发板、开源硬件、嵌入式系统

---

### 4. **MIPS**

* 操作系统：

  * Linux（OpenWRT、Debian MIPS）
  * RTEMS、QNX（实时系统）
  * VxWorks
  * NetBSD
* 典型设备：路由器、交换机、嵌入式系统（逐渐被 RISC-V 取代）

---

### 5. **Power / PowerPC**

* 操作系统：

  * AIX（IBM 自研）
  * Linux（支持 PowerPC 的 Debian、RedHat）
  * AmigaOS（早期经典系统）
  * MorphOS（Amiga 类似系统）
  * macOS（早期 Mac G4、G5）
* 典型设备：IBM 服务器、旧款 Mac、某些游戏机（如 Xbox 360、PS3）

---

### 6. **SPARC**

* 操作系统：

  * Solaris（Sun/Oracle 主打 OS）
  * Linux（Debian SPARC 支持）
  * OpenBSD
* 典型设备：Sun/Oracle 服务器

---

### 7. **Itanium (IA-64)**

* 操作系统：

  * Windows Server（2003、2008 等）
  * Linux（旧版 Red Hat、SUSE）
* 典型设备：HP 和 Intel 的服务器，已基本淘汰

---

## 总结建议

如果你：

* 做通用桌面/服务器开发 👉 **x86\_64**
* 做移动/嵌入式/高能效产品 👉 **ARM/ARM64**
* 研究芯片/开源软硬件 👉 **RISC-V**
* 研究操作系统历史或移植 👉 可了解 MIPS, SPARC, PowerPC

---

如果你感兴趣，我还可以做一张 **思维导图 / 对应图表** 来总结各 ISA 和 OS 的对应关系，要不要？


# 5.3 gdb和汇编代码执行

# 5.4 RISC-V寄存器
![image](../images/Table18.2%20RISC-V%20calling%20convention%20register%20usage..png)

## RISC-V calling convention register usage.

| Register | ABI Name | Description                      | Saver  |
| -------- | -------- | -------------------------------- | ------ |
| x0       | zero     | Hard-wired zero                  | —      |
| x1       | ra       | Return address                   | Caller |
| x2       | sp       | Stack pointer                    | Callee |
| x3       | gp       | Global pointer                   | —      |
| x4       | tp       | Thread pointer                   | —      |
| x5–7     | t0–2     | Temporaries                      | Caller |
| x8       | s0/fp    | Saved register/frame pointer     | Callee |
| x9       | s1       | Saved register                   | Callee |
| x10–11   | a0–1     | Function arguments/return values | Caller |
| x12–17   | a2–7     | Function arguments               | Caller |
| x18–27   | s2–11    | Saved registers                  | Callee |
| x28–31   | t3–6     | Temporaries                      | Caller |
| f0–7     | ft0–7    | FP temporaries                   | Caller |
| f8–9     | fs0–1    | FP saved registers               | Callee |
| f10–11   | fa0–1    | FP arguments/return values       | Caller |
| f12–17   | fa2–7    | FP arguments                     | Caller |
| f18–27   | fs2–11   | FP saved registers               | Callee |
| f28–31   | ft8–11   | FP temporaries                   | Caller |


基本上来说，RISC-V中通常的指令是64bit，但是在Compressed Instruction中指令是16bit。在Compressed Instruction中我们使用更少的寄存器，也就是x8 - x15寄存器。


通常我们在谈到寄存器的时候，我们会用它们的ABI名字。不仅是因为这样描述更清晰和标准，同时也因为在写汇编代码的时候使用的也是ABI名字

为什么s1寄存器和其他的s寄存器是分开的，因为s1在Compressed Instruction是有效的，而s2-11却不是。除了Compressed Instruction，寄存器都是通过它们的ABI名字来引用


当我们调用函数时，你可以看到这里有a0 - a7寄存器。
a0到a7寄存器是用来作为函数的参数。如果一个函数有超过8个参数，我们就需要用内存了。从这里也可以看出，当可以使用寄存器的时候，我们不会使用内存，我们只在不得不使用内存的场景才使用它。




- Caller Saved寄存器在函数调用的时候不会保存
- Callee Saved寄存器在函数调用的时候会保存
- 
一个Caller Saved寄存器可能被其他函数重写。假设我们在函数a中调用函数b，任何被函数a使用的并且是Caller Saved寄存器，调用函数b可能重写这些寄存器。

Return address寄存器（注，保存的是函数返回的地址），你可以看到ra寄存器是Caller Saved，这一点很重要，它导致了当函数a调用函数b的时侯，b会重写Return address。


# 5.5 Stack






