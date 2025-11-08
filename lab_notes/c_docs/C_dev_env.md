# C dev env

## C dev enviroments on Windows

- Windows: MS Visual C++
- Windows: mingw-w64
  pros: 
  cons: 好像有些linux 命令不支持

- Windows: wsl
  
- Windows: remote wsl
  Pros:   
    - without worrying about pathing issues, binary compatibility, or other cross-OS challenges
  

  
- Windows: vm linux 虚拟机
  pros: 使用linux 的系统
  cons: 需要在虚拟机操作，有时候来回切换比较麻烦，如检索，copy
- Windows: docker container
- Windows: remote dev container

  

## C dev tools

- IDE : [vscode](https://code.visualstudio.com/docs/languages/cpp)
- vscode extension: Install recommended C/C++ extension in VSCode and reload
- compiler : MS Visual C++ /gcc/clang


## Compilers

C/C++ extension does not include a C++ compiler. So, you will need to install one or use which is already installed on your computer.

- Windows:  MS Visual C++  or  GCC C++ compiler (g++) +  GDB debugger(using mingw-w64 or Cygwin )
- Linux: gcc + GDB
- Mac: xcode +  LLDB or GDB
Also, Make sure to add C++ compiler PATH to environment variable of your platform. 

For Windows MinGW64 add: C:\MinGW64\bin 


### Issues:

#### Exec format error with MYSYS2 GCC

- Description: why run bomb readme.txt on Windows UCRT64 said: cannot execute binary file: Exec format error?
- Reason: Incompatible Binary Format: 
The bomb file you're trying to execute is probably not a Windows executable file. It might be a binary compiled for Linux, macOS, or another platform (e.g., ELF format for Linux), which Windows cannot directly execute.
Windows uses the Portable Executable (PE) format, while Linux commonly uses the ELF format for binaries.
- Solution
  1. Check the File Format: Use a tool like file (available on Linux or through MSYS2 on Windows) to inspect the binary format of the file. In MSYS2, you can run the following command in the terminal:
  This will tell you if the file is in the wrong format, such as an ELF binary for Linux or a 32-bit binary when your system is 64-bit.

  2. Run on the Correct Platform:

    If the binary is meant for Linux or another Unix-like system, you will need to run it on the correct platform.
    Option 1: Use a Linux machine or a virtual machine (VM) running Linux.
    Option 2: Use Windows Subsystem for Linux (WSL) to run Linux binaries on your Windows machine.




## Unix-like C compilers in Windows

- Cygwin : Cygwin是模拟 POSIX 系统，源码移植 Linux 应用到 Windows 下
- MinGW & MSYS: MinGW 是用于进行 Windows 应用开发的 GNU 工具链（开发环境），它的编译产物一般是原生 Windows 应用
- MinGW-w64 & MSYS2:  https://www.msys2.org/



## WSL Vscode settings
- c_cpp_properties.json (compiler path and IntelliSense settings)
- tasks.json (build instructions)
- launch.json (debugger settings)


- https://code.visualstudio.com/docs/cpp/config-wsl
- https://code.visualstudio.com/docs/cpp/config-mingw
- https://gourav.io/blog/setup-vscode-to-run-debug-c-cpp-code
- https://code.visualstudio.com/docs/cpp/cpp-debug
- https://code.visualstudio.com/docs/cpp/launch-json-reference
- https://code.visualstudio.com/docs/remote/wsl




## Reference
- [Using C++ and WSL in VS Code](https://code.visualstudio.com/docs/cpp/config-wsl)
- https://code.visualstudio.com/docs/remote/wsl
- https://code.visualstudio.com/api/advanced-topics/remote-extensions


## WSL Ubuntu env Installation
 - install wsl and linux distribution
 - install vscode
 - install vscode for local wsl

 To use REMOTE WSL C
 - install WSL extension:


 





## C debug in vscode

- https://code.visualstudio.com/docs/cpp/launch-json-reference


## wsl operarion 

cope/paste :
- win > ubuntu: ctrl+ c/v > right click
- ubuntu > win : ctrl+shift + c  > ctrl+ v
- ubuntu > ubuntu : left click choose > right click


export HTTP_PROXY=[username]:[password]@[proxy-web-or-IP-address]:[port-number]
export HTTP_PROXY=127.0.0.1:7890








# Make

- preprocessing
- compiling
- assembling
- linking

## windows 下使用make命令编译代码

https://blog.csdn.net/Nicholas_Liu2017/article/details/78323391

在 Windows 操作系统中，没有原生的 "make" 命令，但是您可以安装 GNU 工具链来获得它。  
安装 GNU 工具链的方法有很多，其中一种方法是通过安装 MinGW-w64。这是一个在 Windows 平台上提供了许多 GNU 工具的开源项目。在安装 MinGW-w64 后，您可以使用其提供的 "mingw32-make" 命令来执行类似于 Makefile 的任务。  
以下是使用 MinGW-w64 安装 make 命令的步骤：  
1.前往 MinGW-w64 的官方网站 (mingw-w64.org/doku.php/do…  
2.运行安装程序，并按照提示进行安装。请注意，如果您正在运行 64 位的 Windows 操作系统，则应选择与您的操作系统架构相对应的安装程序。  
3.在安装过程中，请确保选择了 "mingw32-base" 和 "mingw32-make" 以及其他您需要的工具。编译器勾选 C compiler 和 C++ compiler  
4.设置环境变量 ： Path 中增加 MinGW/bin  
5.复制或者重命名  "mingw32-make.exe to make.exe   
6.在命令行中输入 "mingw32-make" or make 来执行 Makefile 文件中的任务。  




