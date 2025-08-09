
# References 
- Tools Used in 6.S081 : https://pdos.csail.mit.edu/6.828/2021/tools.html
  

For this class you'll need the RISC-V versions of a couple different tools: QEMU 5.1+, GDB 8.3+, GCC, and Binutils.

If you are having trouble getting things set up, please come by to office hours or post on Piazza. We're happy to help!

## Tools

### QEMU

QEMU (Quick Emulator) is a powerful, open-source machine emulator and virtualizer. It lets you run operating systems and programs for one architecture on a completely different architecture — without needing the actual hardware.

##  What QEMU Can Do

- Emulate entire systems: x86, ARM, RISC-V, MIPS, PowerPC, SPARC, etc.
- Boot and run OS kernels (e.g., Linux, xv6)
- Debug OS kernels or low-level code (with GDB support)
- Mount and manipulate disk images
- Snapshot VM state


## Installing on Windows wsl

First make sure you have the Windows Subsystem for Linux installed. Then add Ubuntu 20.04 from the Microsoft Store. Afterwards you should be able to launch Ubuntu and interact with the machine. To install all the software you need for this class, run:

这只是安装了一些构建 xv6-riscv 所需的工具链（例如编译器、链接器、QEMU 等），但并没有自动安装 xv6 本身，所以 你的系统中还没有 xv6 的目录。

```bash
sudo apt-get update && sudo apt-get upgrade
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

From Windows, you can access all of your WSL files under the "\\wsl$\" directory. For instance, the home directory for an Ubuntu 20.04 installation should be at "\\wsl$\Ubuntu-20.04\home\<username>\".

### QEMU version

WSL 的 Ubuntu 22.04 通过 apt 默认只能安装 QEMU 6.2，而不是最新版本。所以需要你 手动升级 QEMU

```bash
# Ubuntu 22.04 的 jammy-backports 仓库里有更新版本的 QEMU（通常 >=7.2）

echo "deb http://archive.ubuntu.com/ubuntu jammy-backports main restricted universe multiverse" | sudo tee -a /etc/apt/sources.list

sudo apt update
sudo apt -t jammy-backports install qemu-system-misc qemu-system-riscv
# E: Unable to locate package qemu-system-riscv
# Ubuntu 22.04 默认源里 没有单独名为 qemu-system-riscv 的包。
```
solution: wsl --install -d Ubuntu-24.04


## Testing your Installation

To test your installation, you should be able to compile and run xv6 (to quit qemu type Ctrl-a x):


## Xv6, a simple Unix-like teaching operating system

- https://pdos.csail.mit.edu/6.828/2021/xv6.html





 
### Introduction

Xv6 is a teaching operating system developed in the summer of 2006, which we ported xv6 to RISC-V for a new undergraduate class 6.S081.
Xv6 sources and text

The latest xv6 source and text are available via
git clone https://github.com/mit-pdos/xv6-riscv.git
and
git clone https://github.com/mit-pdos/xv6-riscv-book.git



```bash
# 编译用的是 riscv64-linux-gnu-gcc，可以用 make qemu 启动系统。

$ qemu-system-riscv64 --version
# QEMU emulator version 5.1.0

# And at least one RISC-V version of GCC:
riscv64-linux-gnu-gcc --version
# riscv64-linux-gnu-gcc (Debian 10.3.0-8) 10.3.0


git clone https://github.com/mit-pdos/xv6-riscv.git

# To test your installation, you should be able to compile and run xv6 (to quit qemu type Ctrl-a x):
cd xv6-riscv

# This runs xv6 in a RISC-V virtual machine using QEMU. You can then interact with it as if it were real hardware.
# 你运行的 xv6 实际上是一个精简的 Unix 类操作系统，它运行在 QEMU 模拟的 RISC-V 机器上。
make qemu

# ... lots of output ...
init: starting sh

# xv6 的 shell 是自带的，非常简单，可以运行如 ls, cat, echo, kill, ps, forktest, stressfs, usertests 等命令。
# 你可以在 xv6 shell 下运行 usertests 进行自动测试：
usertests

# 4. 输入 ctrl-a x （退出 QEMU）
# 如果你要退出 xv6/QEMU，按：
# Ctrl + a  然后按 x

```




