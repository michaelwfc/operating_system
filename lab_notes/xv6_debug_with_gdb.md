# xv6 introduction

## file structure
- kernel/

v
sh.c
sh.asm
sh.o
sh.d
_sh 

# QEMU

## QEMU 介绍
QEMU 是一个很“全能”的虚拟化工具，严格说它是 开源的仿真器（emulator）和虚拟机（virtualizer）。
QEMU is a machine emulator/virtualizer.

QEMU 是一个很“全能”的虚拟化工具，严格说它是 **开源的仿真器（emulator）和虚拟机（virtualizer）**。

💡 可以分成两大功能：

---

### 1. 仿真器 (Emulator)

* QEMU 可以**模拟不同的 CPU 架构**。
* 举例：你在 x86 的 PC 上，也能用 QEMU 运行 ARM、MIPS、RISC-V 等架构的系统或程序。
* 在这个模式下，QEMU 会逐条“翻译”指令，所以性能比不上原生。

常用于：

* 开发交叉架构的软件（比如在 PC 上调试嵌入式 ARM 代码）。
* 做一些实验，比如学操作系统、测试 bootloader。

---

### 2. 虚拟机 (Virtualizer)

* 如果 guest OS 和 host OS **架构相同**（比如都是 x86\_64），QEMU 可以调用 Linux 的 **KVM（Kernel-based Virtual Machine）**。
* 这样 guest 的指令基本就直接在硬件上跑，性能接近原生。

常用于：

* 跑虚拟机（类似 VirtualBox/VMware）。
* 在 Linux 上启动一个完整的 Linux/Windows 虚拟机。

---

### 3. 其他功能

* QEMU 能模拟 **各种硬件设备**（串口、网卡、硬盘、显卡、USB…）。
* 可以结合 **GDB** 做内核/驱动调试。
* 在操作系统课程（比如 MIT 6.S081、CS\:APP）里，QEMU 常常被用来跑实验环境，因为它能模拟硬件启动过程。

---

⚡一句话总结：
**QEMU = 一把瑞士军刀**

* 想模拟别的 CPU → 它是仿真器。
* 想跑虚拟机 → 它是虚拟化器。
* 想学内核/操作系统 → 它是实验神器。

---

要不要我帮你画一张“QEMU 在仿真、虚拟化、调试三种场景下的关系图”，让你一眼就能看懂它的定位？


In xv6 labs, you use qemu-system-riscv64 to emulate a RISC-V machine that boots your kernel/kernel binary.

QEMU has different “front-ends” for I/O:
- nographic mode → redirects serial console (xv6’s printf, shell, etc.) into your terminal.
- monitor → a QEMU command console (to control VM, e.g. info registers, xp, quit).


## Start qemu/qemu-gdb
```bash

make qemu-gdb 
# This runs QEMU and pauses at startup, listening on port 26000  for GDB connections.

# or explicitly:
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -S -m 128M -smp 1 -monitor stdio -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -S -gdb tcp::26000

# qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -S -gdb tcp::26000

# -monitor stdio: to access the QEMU monitor in qemu-gdb mode
# -S → pause CPU at startup
# Since GDB is attached to the full kernel memory, it can actually stop in the kernel trap handler. Use si or stepi to step instruction by instruction inside kernel code.

man qemu-system-riscv64 
# ctrl a+c 进入 qemu monitor
(qemu) 

# 查看 make 命令detail
make -nB qemu | vim -

# 根据需要修改一些配置： 优化等级，CPU 数，编译指令

```




## info mem
qemu has a "monitor" that lets you query the state of the emulated machine. You can get at it by typing control-a c (the "c" is for console). A particularly useful monitor command is info mem to print the page table. You may need to use the cpu command to select which core info mem looks at, or you could start qemu with make CPUS=1 qemu to cause there to be just one core.

## commands

### Access QEMU monitor

Normally, when QEMU is in **“graphical” mode**, Ctrl-a c toggles between serial console and QEMU monitor.
But in xv6 labs, we run with -nographic, which disables the graphical console. In this mode, Ctrl-a c is intercepted by stdio redirection and doesn’t open the monitor.

Option 1. Use -monitor stdio instead of -nographic (monitor replaces console)

Edit the xv6 Makefile (or override on the command line) so QEMU runs with:

```bash
qemu-system-riscv64 ... -monitor stdio
(qemu)
```
Kernel output goes to serial (-serial mon:stdio)

QEMU monitor takes commands directly from stdin
You’ll see a (qemu) prompt where you can type info mem.

This gives you the QEMU monitor directly in the terminal.
Downside: you lose the xv6 console output in the same terminal.

### Quit qemu
ctrl-a x


# GDB basic


## What are Symbols in GDB?
Symbols are the mapping between:
- Human-readable names (like function names, variable names)
- Memory addresses (where those functions/variables actually exist)

Without Symbols:
```bash
(gdb) x/10i $pc
=> 0x1234:    li    a7,16
   0x1236:    ecall  
   0x123a:    ret
# You only see addresses and assembly - no function names!

```

With Symbols:
```bash
(gdb) x/10i $pc  
=> 0x1234 <write>:       li    a7,16
   0x1236 <write+2>:     ecall
   0x123a <write+6>:     ret
# Now you see function name "write" and offsets!
```

### Symbol Information Includes:

Function names → memory addresses
Variable names → memory locations
Source file names → code locations
Line numbers → instruction addresses
Data types → memory layouts



### ELF Binary Structure:
```bash
user/_sh (ELF file):
├── Code sections (.text)
├── Data sections (.data, .bss)  
├── Symbol table (.symtab)       ← This is what GDB reads!
├── String table (.strtab)
└── Debug information (.debug_*)


# Example symbol table entries:
Address    Size  Type    Name
0x000100    24   FUNC    main
0x000200    48   FUNC    getcmd
0x000300    12   FUNC    runcmd
0x001000    4    OBJECT  global_var
```

### What add-symbol-file user/_sh 0x0 Means:
This command tells GDB:
- add-symbol-file: Add symbols from a file (don't replace existing ones)
- user/_sh: The binary file containing symbol information
- 0x0: The base address where these symbols should be loaded, 

```bash
add-symbol-file user/_sh 0x0
The 0x0 tells GDB where the symbols are located in memory:

# Now GDB knows:
# - main function in binary at offset 0x100 → actual address 0x1100  
# - getcmd function in binary at offset 0x200 → actual address 0x1200
```

## GDB setting .gdbinit

### 1. Set xv6_labs_main/.gdbinit
```bash
cat ~/.gdbinit 
add-auto-load-safe-path /mnt/e/projects/operating_system/xv6_labs_main/.gdbinit
source /mnt/e/projects/operating_system/xv6_labs_main/.gbdinit_my

# xv6_labs_main/.gdbinit_my
set confirm off
set architecture riscv:rv64
symbol-file kernel/kernel
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes
set disassemble-next-line on
set print pretty on 
set print array on 

# Useful gdb settings for C in xv6
# set disassemble-next-line on     # show asm after each step
# set print pretty on              # pretty-print structs
# set print array on               # expand arrays
# set pagination off


```

Nice — let’s go line by line through that `.gdbinit`. It’s basically a startup script for GDB so you don’t have to type everything manually each time.

---

 1. `set confirm off`

* Disables confirmation prompts like *"Are you sure you want to quit?"*
* Useful in scripted/automated sessions (so `quit` doesn’t stop to ask).

---

2. `set architecture riscv:rv64`

* Tells GDB the target architecture explicitly.
* `riscv:rv64` = 64-bit RISC-V (RV64I base ISA).
* Without this, GDB usually guesses, but setting it avoids “Unknown architecture” errors when attaching to QEMU.

---

3. `target remote 127.0.0.1:26000`

* Connects GDB to a remote target via TCP (QEMU’s built-in GDB stub).
* `127.0.0.1:26000` → QEMU is listening locally on port `26000`.
* Matches how you ran QEMU with `-S -gdb tcp::26000`.
* After this line, GDB controls the QEMU VM.

---

4. `symbol-file kernel/kernel`

* Loads symbol information from the xv6 kernel ELF binary (`kernel/kernel`).
* This allows you to set breakpoints by function name (`b usertrap`) instead of raw addresses.
* Also enables `info functions`, source line mapping, etc.

---

5. `set disassemble-next-line auto`

* Controls whether GDB disassembles instructions at the current PC after each step.
* `auto` → GDB shows assembly if there’s no source code available (typical for trampoline or hand-written assembly).
* Very handy in xv6 because lots of early code (boot, trap, trampoline) has no C source.

---

6. `set riscv use-compressed-breakpoints yes`

* (typo in your snippet: should be `riscv`, not `risv` 😉)
* RISC-V has 16-bit “compressed” instructions (`C` extension).
* Breakpoints must be encoded differently depending on whether the instruction at that address is 16-bit or 32-bit.
* Setting this to `yes` tells GDB: *“it’s safe to patch breakpoints into compressed instructions”*.
* Without this, GDB might refuse to set a breakpoint on a 16-bit instruction.

---

✅ In short, this `.gdbinit` does:

1. Remove confirmation prompts.
2. Tell GDB the target is RV64.
3. Connect to QEMU at port 26000.
4. Load xv6 kernel symbols.
5. Show disassembly automatically when no C source is available.
6. Allow breakpoints even on compressed RISC-V instructions.
   

### 2. Configuration file "/home/michael/.gdbinit".



```bash
# For help, type "help".
# Type "apropos word" to search for commands related to "word"...
# --Type <RET> for more, q to quit, c to continue without paging--
# Reading symbols from kernel/kernel...
# warning: File "/mnt/e/projects/operating_system/xv6_labs_main/.gdbinit" auto-loading has been declined by your `auto-load safe-path' set to "$debugdir:$datadir/auto-load".
# To enable execution of this file add
#         add-auto-load-safe-path /mnt/e/projects/operating_system/xv6_labs_main/.gdbinit
# line to your configuration file "/home/michael/.gdbinit".
# To completely disable this security protection add
#         set auto-load safe-path /
# line to your configuration file "/home/michael/.gdbinit".
# For more information about this security protection see the
# "Auto-loading safe path" section in the GDB manual.  E.g., run from the shell:
#         info "(gdb)Auto-loading safe path"

cd ~/.gdbinit
cat .gdbinit 
add-auto-load-safe-path /mnt/e/projects/operating_system/xv6_labs_main/.gdbinit

```





## Invoking gdb

- https://pdos.csail.mit.edu/6.828/2021/labs/guidance.html

For kernel or user-space debugging:

1. 启动 QEMU with GDB server:
In one terminal:
```bash
cd xv6-riscv
make qemu-gdb 

# *** Now run 'gdb' in another window.
init: starting sh
$
# xv6 第一個執行的程式是 sh，它會先 print 出字元 $ print 的過程需要使用 system call write
```

2. 启动 GDB：
Load the debug symbols for your program 
In a separate terminal:


```bash
# *** Now run 'gdb' in another window.
# riscv64-linux-gnu-gdb / riscv64-unknown-elf-gdb
gdb-multiarch
# This is just the GNU debugger build that supports multiple architectures (ARM, RISC-V, MIPS, etc.). You need this since xv6 is running on RISC-V, not x86.

# You start GDB without loading any program. You get the (gdb) prompt.
# At this point, GDB is idle, and you can:
# - Use file <program> to load an ELF binary
# - Use target remote ... to attach to a remote target (e.g., QEMU)
# - Set breakpoints, load symbols, inspect memory, etc.
# Essentially: it’s like starting an empty debugger shell.

(gdb)

# user/_primes is a RISC-V ELF
# loaded the user/_primes binary ,just loads debug symbols into GDB
# it hasn’t actually started xv6 or loaded your program into the emulator’s memory yet.
(gdb) file user/_find



# if we want to dubug kernel code
# Here, you’re invoking GDB and immediately telling it to load an ELF file called kernel/kernel
# GDB does two things automatically:
# - Loads the symbols from the ELF file (kernel/kernel)
# - Sets the entry point (program counter) to the _start of that ELF
# You can now run start, run, break main, or si directly on that program.

gdb-multiarch kernel/kernel
# Now GDB is connected to the running xv6 kernel in QEMU, not just a user-space ELF.

# GNU gdb (Ubuntu 9.2-0ubuntu1~20.04.2) 9.2
# Copyright (C) 2020 Free Software Foundation, Inc.
# License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
# This is free software: you are free to change and redistribute it.
# There is NO WARRANTY, to the extent permitted by law.
# Type "show copying" and "show warranty" for details.
# This GDB was configured as "x86_64-linux-gnu".
# Type "show configuration" for configuration details.
# For bug reporting instructions, please see:
# <http://www.gnu.org/software/gdb/bugs/>.
# Find the GDB manual and other documentation resources online at:
#     <http://www.gnu.org/software/gdb/documentation/>.

(gdb) info target
(gdb) info files                # Show all loaded symbol files
(gdb) info symbol-file          # Show main symbol file  
(gdb) info shared               # Show additional symbol files


# This attaches GDB to QEMU
(gdb) target remote :26000 





# Remote debugging using :26000
# 0x0000000000001000 in ?? ()

# set a break point, followed by followed by 'c' (continue), and xv6 will run until it hits the breakpoint.
(gdb) b main
# Breakpoint 1 at 0x1ec: file user/primes.c, line 79.
(gdb) c
# Continuing.

# You can set breakpoints in kernel functions, e.g., sys_write or trap handler:
(gdb) break sys_write
(gdb) break trap

```

3. Run _primes inside xv6

In the QEMU console (still running in the make qemu-gdb terminal), type:
```bash

$ make qemu-gdb
# *** Now run 'gdb' in another window.
# qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -S -gdb tcp::26000

# xv6 kernel is booting

# hart 2 starting
# hart 1 starting
# prinit: starting sh

$ primes
# When _primes starts, GDB will hit your breakpoint
```


3. Set a breakpoint:
 
```bash
Thread 3 hit Breakpoint 1, main (argc=1, argv=0x2fe0) at user/primes.c:79
79      {
```


### Starts GDB with no target initially
```bash

gdb-multiarch
(gdb) file kernel/kernel      # Start with kernel


gdb-multiarch kernel/kernel
```

### Kernel debugging
#### gdb-multiarch kernel/kernel
- Primary target: Kernel debugging
- Symbol table: Loads kernel symbols (functions like sys_write, usertrap, - copyout)
- Execution Context: 
  - Running in supervisor/machine mode
  - Kernel memory context
  - Kernel stack context
  - kernel registers context

- Memory context: Can see kernel memory, page tables, etc. 
  - Kernel's virtual address space layout
  - GDB expects addresses like: 0x80000000+ (kernel virtual addresses)
  - Wrong address space interpretation: Kernel GDB interprets addresses as kernel addresses
  - Cannot reliably access user space when user process is running
- NO page tables: Just knows about kernel address space structure

#### add-symbol-file user/_sh
- Adds symbols without replacing current target
- Keeps existing symbols (kernel + user symbols both available)
- Requires explicit load address (0x1000 in example)
- Multiple symbol tables can coexist
- Use case: When debugging both kernel and user code

#### file user/_sh
- Replaces the current target completely
- Unloads previous symbols (kernel symbols are gone)
- Sets user/_sh as the main executable being debugged
- Address base: Assumes default load address (usually 0x0)
- Use case: When you want to debug ONLY the user program
  
  Problem: 
  - Still connected to same QEMU instance in same execution kernel context
  - Change symbols but NOT execution context

### User process debugging
``` bash
set confirm off
set architecture riscv:rv64
symbol-file kernel/kernel   Load kernel as MAIN target, Sets OBJF_MAINLINE flag
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes


# add-symbol-file user/_sh        # user/_sh = ADDITIONAL
# then
# gdb-multiarch user/_sh          # Try to make user/_sh = MAIN
# ❌ But user/_sh is already loaded as ADDITIONAL!
# ❌ Conflict between MAIN and ADDITIONAL flags for same file
```

#### gdb-multiarch user/_sh

```bash
# Status: user/_sh = NEW MAIN target (replaces kernel as main)
# sets OBJF_MAINLINE flag
gdb-multiarch user/_sh

(gdb) info target
Symbols from "/mnt/e/projects/operating_system/xv6_labs_main/kernel/kernel".
Remote serial target in gdb-specific protocol:
Debugging a target over a serial line.
        While running this, GDB does not access memory from...
Local exec file:
        `/mnt/e/projects/operating_system/xv6_labs_main/user/_sh',
        file type elf64-littleriscv.
        Entry point: 0xa60
        0x0000000000000000 - 0x000000000000137a is .text
        0x0000000000001380 - 0x0000000000001504 is .rodata
        0x0000000000001508 - 0x0000000000001516 is .sdata
        0x0000000000001518 - 0x0000000000001520 is .sbss
        0x0000000000001520 - 0x0000000000001598 is .bss
        0x0000000000000000 - 0x000000000000137a is .text
        0x0000000000001380 - 0x0000000000001504 is .rodata
        0x0000000000001508 - 0x0000000000001516 is .sdata
        0x0000000000001518 - 0x0000000000001520 is .sbss
        0x0000000000001520 - 0x0000000000001598 is .bss
```

- Primary target: User process debugging
- Sets user/_sh as the main executable being debugged
- Symbol table: Loads shell symbols (functions like getcmd, runcmd, main)
- Execution Context: 
  - Running in user mode
- Memory view: 
  - Can see user process memory, stack, heap. 
  - User process virtual address space (0x0 - 0x7fffffff typically)
  - GDB expects addresses like:  0x0 - 0x7fffffff (user virtual addresses)
  - Can access user process data
- NO page tables: Just knows about user address space structure






## kernel/kernel.asm

If you want to see what the assembly is that the compiler generates for the kernel or to find out what the instruction is at a particular kernel address, see the file kernel.asm, which the Makefile produces when it compiles the kernel. (The Makefile also produces .asm for all user programs.)


If the kernel panics, it will print an error message listing the value of the program counter when it crashed; you can search kernel.asm to find out in which function the program counter was when it crashed, or you can run addr2line -e kernel/kernel pc-value (run man addr2line for details). If you want to get backtrace, restart using gdb: run 'make qemu-gdb' in one window, run gdb (or riscv64-linux-gnu-gdb) in another window, set breakpoint in panic ('b panic'), followed by followed by 'c' (continue). When the kernel hits the break point, type 'bt' to get a backtrace.

If your kernel hangs (e.g., due to a deadlock) or cannot execute further (e.g., due to a page fault when executing a kernel instruction), you can use gdb to find out where it is hanging. Run run 'make qemu-gdb' in one window, run gdb (riscv64-linux-gnu-gdb) in another window, followed by followed by 'c' (continue). When the kernel appears to hang hit Ctrl-C in the qemu-gdb window and type 'bt' to get a backtrace.





# xv6 c code debug with VSCode
## 1. install tools
sudo apt install -y build-essential git make qemu-system-riscv64 gdb-multiarch clangd bear \
                    clang-format python3-pip

## 2. 在项目里生成 compile_commands.json

进入 xv6 源码目录（含 Makefile）：
```bash
make clean
bear make
```
生成后会在工程根目录出现 compile_commands.json，clangd / IDE 会用它做精确的 include/flag 信息。

## 3. 安装 vscode extensions
extensions.json
```json
{
    "recommendations": [
        "ms-vscode.cpptools",
        "llvm-vs-code-extensions.vscode-clangd",
        "ms-vscode.cpptools-extension-pack",
        "webfreak.debug"
    ]
}
```
## 4. 在 workspace 下创建 .vscode, 
### 配置 launch.json
.vscode/launch.json
用于 attach 到 QEMU 的 GDB stub（不直接启动 QEMU）：
说明：
- 用 attach 到 QEMU 在 :26000 的 GDB stub（这个端口与 make qemu-gdb 中使用的示例一致）。
- program 指向 kernel ELF（用于符号解析）。
- setupCommands 用来设置架构并加载符号、允许 pending 断点。
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Attach to QEMU (gdb-multiarch)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/xv6_labs_main/kernel/kernel",
      "cwd": "${workspaceFolder}/xv6_labs_main",
      "miDebuggerServerAddress": "127.0.0.1:26000",
      "miDebuggerPath": "/usr/bin/gdb-multiarch",
      "MIMode": "gdb",
      "setupCommands": [
        {
          "description": "set riscv architecture",
          "text": "set architecture riscv:rv64",
          "ignoreFailures": false
        },
        {
          "description": "load kernel symbols",
          "text": "symbol-file ${workspaceFolder}/kernel/kernel",
          "ignoreFailures": true
        },
        {
          "description": "enable pending breakpoints",
          "text": "set breakpoint pending on",
          "ignoreFailures": true
        }
      ],
      "logging": {
        "engineLogging": false,
        "trace": false
      }
    }
  ]
}
```

###  配置 .vscode/.vscode/tasks.json
### .vscode/settings.json
一些工作区设定（clangd、C++ extension）：
说明：clangd 会自动读取 compile_commands.json，若没放在根目录可用 --compile-commands-dir 指定目录。
```json
{
  "clangd.path": "/usr/bin/clangd",
  "clangd.arguments": ["--compile-commands-dir=${workspaceFolder}"],
  "C_Cpp.intelliSenseEngine": "Default",
  "C_Cpp.default.compilerPath": "/usr/bin/clang",
  "C_Cpp.default.configurationProvider": "ms-vscode.cpptools",
  "files.exclude": {
    "**/.git": true
  }
}
```

## 5 设置 .gdbinit（放在项目根或你的 HOME）
这是 GDB 启动时的便捷配置（示例）：
说明：
我把 target remote 注释掉，因为在 VSCode attach 时会由 debugger 自己处理连接；如果你在外面直接用 gdb-multiarch，可以把 target remote 取消注释。

```json
set confirm off
set pagination off
set disassemble-next-line auto
set architecture riscv:rv64
set riscv use-compressed-breakpoints yes

# Connect to QEMU if started with -gdb tcp::26000
# target remote 127.0.0.1:26000

# Load kernel symbols (if you start gdb with "gdb-multiarch kernel/kernel" this is redundant)
# symbol-file kernel/kernel

# Useful: allow pending breakpoints for symbols not yet mapped
set breakpoint pending on

```

## 6. C debug 工作流

1. 在终端（或 VSCode 的外部终端）启动 QEMU（gdb stub，暂停在启动态）：
```bash
# 启动 QEMU with GDB server
# make qemu-gdb 会启动 QEMU 并监听 GDB（通常会阻塞在启动，等待 GDB 连接）。建议在外部终端启动它（或在 VSCode 终端手动运行该任务）。
make qemu-gdb
```

2. 在另一个终端生成 compile_commands.json（如果还没生成）：

   如果要用 clangd 给 VSCode 提供智能提示，还可以配合 compile_commands.json。
   clangd: 提供 C/C++ 语言服务器（VSCode、Vim、IDEA 都依赖它）。


3. 在 VSCode 中打开工作区，确保 clangd 与 C++ 插件安装并激活（会基于 compile_commands.json 提供跳转/语法检查）

4. 在 VSCode 的 Run/Debug 面板选择 Attach to QEMU (gdb-multiarch)，点击 Start Debugging（或 F5）。它会：
- 启动 gdb-multiarch 并连接到 localhost:26000
- 加载 kernel symbols
- 设置 set breakpoint pending on

1. 在用户源或 kernel 源设置断点，例如：

- 用户侧 write 的 stub（若你知道地址：b *0xdfe）
- kernel:b main、 b usertrap、b uservec（若 uservec 还未可访问，pending 会生效）

6. 回到 xv6 shell（QEMU 的串口窗口），运行一个会触发 syscall 的命令（如 echo hi），观察 VSCode 是否在断点停下。用 stepi（单条指令）逐步进入 ecall → trampoline → usertrap。