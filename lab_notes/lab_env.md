
# References 
- Tools Used in 6.S081 : https://pdos.csail.mit.edu/6.828/2021/tools.html

# wsl

## wsl Ubuntu-20.04

```bash
sudo apt-get update && sudo apt-get upgrade

sudo apt install -y build-essential gcc gdb make libssl-dev zlib1g-dev libbz2-dev libreadline-dev libsqlite3-dev wget curl llvm libncurses5-dev libncursesw5-dev xz-utils tk-dev libffi-dev liblzma-dev libcurl4-openssl-dev

# 推出当前 wsl
wsl -d Ubuntu-24.04

# WSL mounts Windows drives under /mnt, so E:\ becomes /mnt/e.
cd /mnt/e/projects/operating_system
# This opens the current folder (/mnt/e/projects/operating_system) in VS Code, using the WSL Ubuntu-24.04 environment.
code .
```

# Tools
For this class you'll need the RISC-V versions of a couple different tools: QEMU 5.1+, GDB 8.3+, GCC, and Binutils.

If you are having trouble getting things set up, please come by to office hours or post on Piazza. We're happy to help!




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


# 编译用的是 riscv64-linux-gnu-gcc，可以用 make qemu 启动系统。
$ qemu-system-riscv64 --version
# QEMU emulator version 4.2.1 (Debian 1:4.2-3ubuntu6.30)
# Copyright (c) 2003-2019 Fabrice Bellard and the QEMU Project developers

# And at least one RISC-V version of GCC:
riscv64-linux-gnu-gcc --version
# riscv64-linux-gnu-gcc (Ubuntu 9.4.0-1ubuntu1~20.04) 9.4.0
# Copyright (C) 2019 Free Software Foundation, Inc.
# This is free software; see the source for copying conditions.  There is NO
# warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

```

From Windows, you can access all of your WSL files under the "\\wsl$\" directory. For instance, the home directory for an Ubuntu 20.04 installation should be at "\\wsl$\Ubuntu-20.04\home\<username>\".




# xv6-labs-2021

```bash
https://github.com/PKUFlyingPig/MIT6.S081-2020fall.git

# $ git clone git://g.csail.mit.edu/xv6-labs-2021
# Cloning into 'xv6-labs-2021'...

# git clone https://github.com/mit-pdos/xv6-riscv.git
...
$ cd xv6-labs-2021
git branch -a


$ git checkout util
# Branch 'util' set up to track remote branch 'util' from 'origin'.
# Switched to a new branch 'util'

git log --oneline
git log --pretty=format:"%h %an %s"
# %h → abbreviated commit hash
# %an → author name
# %s → commit subject

# Temporary "time travel"
# 1. Checkout the commit (detached HEAD)
git checkout <commit-hash>
# Purpose: Temporarily move HEAD to a past commit (detached HEAD state,you’re not on a branch).
# Effect:
# - You can look at the repo as it was at that commit.
# - Doesn’t delete history, doesn’t rewrite commits.
# - If you make changes, you must create a new branch to save them: 
git checkout -b debug-old-version
# Analogy: “Travel back in time, look around, but don’t change history.”

#  temporary "time travel" using git switch --detach
git switch --detach <commit-hash>

# Note: switching to '773d70b'.

# You are in 'detached HEAD' state. You can look around, make experimental
# changes and commit them, and you can discard any commits you make in this
# state without impacting any branches by switching back to a branch.

# If you want to create a new branch to retain commits you create, you may
# do so (now or later) by using -c with the switch command. Example:

#   git switch -c <new-branch-name>

# Or undo this operation with:

#   git switch -

# Turn off this advice by setting config variable advice.detachedHead to false

# HEAD is now at 773d70b fix filename



git revert <commit-hash>
# Purpose: Safely undo a commit by creating a new commit that reverses the changes.
# Effect: Keeps history intact, makes collaboration safe (no rewrite).
# Analogy: “Instead of erasing history, write a new chapter that says ‘undo what happened earlier.’”


git reset
# Purpose: Move the branch pointer (HEAD) to another commit.
# Types:
# --soft: Moves HEAD, keeps changes staged (in index).
# --mixed (default): Moves HEAD, keeps changes in working dir, unstaged.
# --hard: Moves HEAD and resets working dir and index (⚠️ destructive).
# Effect: Can erase commits from history (locally). If you git push --force, you rewrite remote history too.
# Analogy: “Rewind history to an earlier point and optionally throw away or keep your changes.”

# Resetting back to a orginal commit
git reflog
git reset --hard HEAD@1

```

# Testing your Installation

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


# To test your installation, you should be able to compile and run xv6 (to quit qemu type Ctrl-a x):
cd xv6-labs-2021

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

# QEMU version

### wsl Ubuntu-22.04 QEMU version

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


### wsl Ubuntu-24.04
```bash
wsl -l -o
wsl --install -d Ubuntu-24.04
wsl -d Ubuntu-24.04
lsb_release -a
# 要把 Ubuntu 24.04（Noble Numbat）的软件源（apt 仓库）更换为阿里云镜像，你需要修改系统的 APT 源配置文件 /etc/apt/sources.list
# 你看到的提示说明 从 Ubuntu 23.10 开始（包括 24.04），APT 的软件源配置方式已经变更：
# sources.list 不再是主要配置文件，而是迁移到了新的 deb822 格式配置文件中，位置为：
# /etc/apt/sources.list.d/ubuntu.sources
# sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak
sudo cp /etc/apt/sources.list.d/ubuntu.sources  /etc/apt/sources.list.d/ubuntu.sources.bak

# manually
sudo nano /etc/apt/sources.list.d/ubuntu.sources
# 2. 找到并修改 URL 字段（原默认是 archive.ubuntu.com）
URIs: http://archive.ubuntu.com/ubuntu or  URIs: http://security.ubuntu.com/ubuntu
URIs: http://mirrors.aliyun.com/ubuntu

# 如果你使用的是 nano，按 Ctrl + O 保存，按回车确认，Ctrl + X 退出。

# 替换的方法自动更改
sudo sed -i 's|http://archive.ubuntu.com/ubuntu|http://mirrors.aliyun.com/ubuntu|g' /etc/apt/sources.list.d/ubuntu.sources
sudo sed -i 's|http://security.ubuntu.com/ubuntu|http://mirrors.aliyun.com/ubuntu|g' /etc/apt/sources.list.d/ubuntu.sources

sudo apt update|grep aliyun
```