# gdb (GNU Debugger)

- Purpose: gdb is an interactive debugger that allows you to run a program, inspect its behavior in real-time, set breakpoints, step through code, and examine variables and memory.
- Functionality: It provides control over the execution of your program, allowing you to pause, step through code, change variable values, and diagnose runtime issues.
- Interactive: gdb is designed for interactive use, meaning you can enter commands while your program is running and immediately see the results.


The GNU debugger, this is a command line debugger tool available on virtually every platform. You
can trace through a program line by line, examine memory and registers, look at both the source code
and assembly code (we are not giving you the source code for most of your bomb), set breakpoints,
set memory watch points, and write scripts.

## Resources:
- The CS:APP web site
http://csapp.cs.cmu.edu/public/students.html
- Beej's Quick Guide to GDB (based on the very handy gdb -tui mode)
  https://beej.us/guide/bggdb/
- Two-page x86-64 GDB cheat sheet
- 

– To keep the bomb from blowing up every time you type in a wrong input, you’ll want to learn how to set breakpoints.
– For online documentation, type “help” at the gdb command prompt, or type “man gdb”,
or “info gdb” at a Unix prompt. Some people also like to run gdb under gdb-mode in
emacs.

## 3.10.2 Using the gdb Debugger

Figure 3.39 Example gdb commands. These examples illustrate some of the ways gdb supports debugging of machine-level programs.

|Command                         |Effect                                                       |
|--------------------------------|--------------------------------------------------------------|
|***Starting and stopping***           |                                                             |
|quit                            |Exit gdb                                                 |
|run                             |Run your program (give command-line arguments here)      |
|kill                            |Stop your program     |
|Breakpoints                     |     |
|break multstore                 |Set breakpoint at entry to function multstore     |
|break *0x400540                 |Set breakpoint at address 0x400540     |
|delete 1                        |Delete breakpoint 1     |
|delete                          |Delete all breakpoints     |
|***Execution***                       |                                                             |
|stepi                           |Execute one instruction     |
|stepi 4                         |Execute four instructions     |
|nexti                           |Like stepi, but proceed through function calls     |
|continue                        |Resume execution     |
|finish                          |Run until current function returns     |
|***Examining code***                  |                                                             |
|disas                           |Disassemble current function     |
|disas multstore                 |Disassemble function multstore     |
|disas 0x400544                  |Disassemble function around address 0x400544     |
|disas 0x400540, 0x40054d        | Disassemble code within specified address range     |
|print /x $rip                   |Print program counter in hex     |
|***Examining data***                |                                                             |  
|print $rax                      |Print contents of %rax in decimal     |
|print /x $rax                   |Print contents of %rax in hex     |
|print /t $rax                   |Print contents of %rax in binary     |
|print 0x100                     |Print decimal representation of 0x100     |
|print /x 555                    |Print hex representation of 555     |
|print /x ($rsp+8)               |Print contents of %rsp plus 8 in hex     |
|print \*(long\*) 0x7fffffffe818  |Print long integer at address 0x7fffffffe818     |
|print \*(long\*) ($rsp+8)        |Print long integer at address %rsp + 8     |
|x/2g 0x7fffffffe818             |Examine two (8-byte) words starting at address 0x7fffffffe818     |
|x/20b multstore                 |Examine first 20 bytes of function multstore     |
|***Useful information***              |                                                             |
|info frame                      |Information about current stack frame     |
|info registers                  |Values of all the registers     |
|help                            |Get information about gdb     |


## 1. Compile the Program with Debugging Information

To debug a C program using gdb, you need to compile your program with the -g flag, which includes debugging information in the executable.

```bash
gcc -g -o myprogram myprogram.c
```
This command compiles myprogram.c into an executable named myprogram, including debugging symbols.

##  2. Start gdb

```shell
# Launch gdb with your compiled program:
gdb bomb
>(gdb)
```

## 3. Run the Program

```shell
# To start running your program inside gdb, use the run command. You can also pass arguments to your program if needed:
(gdb) run
(gdb) run arg1 arg2

# run with arguments from answers.txt
(gdb) r < answers.txt
```

## 4. Set Breakpoints

```bash
# add a breakpoint line to the debugger
(gdb) break 74
(gdb) b 74
Breakpoint 1 at 0x400e37: file bomb.c, line 74.

# Set a breakpoint at a specific function entry:
(gdb) break main
(gdb) b main


# List breakpoints to find their numbers: info breakpoints
(gdb) info breakpoints

# Delete a specific breakpoint: delete <breakpoint-number>
(gdb) delete 2


#Delete all breakpoints: delete
(gdb) delete
```

## 5. Step Through the Code

```bash
# Once a breakpoint is hit, you can step through the code to observe how it executes:

# Next Line (next): Executes the next line of code, but doesn’t step into functions.
(gdb) next

# Executes the next line of instructions
(gdb) nexti
(gdb) ni

(gdb) until *<instruction memeory>

# Step Into (step): Steps into the function calls to see what happens inside them.
(gdb) step
# if the current line is a function call, step will take you inside that function, allowing you to debug the function line by line.
# If the current line is not a function call, step will simply move to the next line of code in the current function.


(gdb) stepi
# step next instruction


# Finish (finish): Runs until the current function returns.
# If you accidentally step into a function and want to finish it quickly, you can use the finish command to run until the current function returns.
(gdb) finish


# Continue (continue): Continues running the program until the next breakpoint or the end of the program.
(gdb) continue

```

## 6. Inspect Variables

The p (or print) command in gdb is used to evaluate and print the value of an expression, which could be a variable, a complex expression, or a memory address.

```bash
(gdb) p <expression>

# To check the values of variables, you can use the print command:
(gdb) print x
#This will display the current value of the variable x.

# Print expression: You can also evaluate expressions:
(gdb) print x + y

# Dereference a pointer: If ptr is a pointer, this prints the value stored at the memory location ptr points to.
(gdb) p *ptr

# Print the address of a variable: This prints the memory address where the variable x is stored.
(gdb) p &x

# You can also directly print the value of %rdx using the print (or p for short) command with the $ prefix to indicate a register.
(gdb) print $rdx
# To print the same value as a character:
(gdb) p (char)$ecx
# Print the Value in Binary Format:
(gdb) print/t $eax
(gdb) print/t 0xF3

# print the binary representation to character
print (char)0b1000001

# print` 命令打印字符串
p (char*)input_pointer

# 直接通过变量名查看字符串内容
(gdb) x/s input_pointer
# 0x55555555a000: "Hello, GDB!"

# 或者通过地址查看
(gdb) x/s &input_pointer
# 0x55555577b018: 0x55555555a000   → 0x55555555a000: "Hello, GDB!"

# Format Specifiers in GDB:
# /t - binary
# /x - hexadecimal
# /d - decimal
# /o - octal
```



##  6.1  Inspect memory

The x command in gdb is used to examine memory. Examine memory (x): View memory at a specific address:

```bash
# - x/d <address>: View the memory as a signed integer.
# - x/u <address>: View the memory as an unsigned integer.
# - x/f <address>: View the memory as a floating-point number.
# - x/s <address>: View the memory as a null-terminated string.
# - x/<count>d <address>: View an array of integers.
# - x/<count>f <address>: View an array of floats.

# <count> specifies the number of units to display (optional).
# <format> specifies how to display the memory contents (e.g., as an integer, character, etc.).
# <address> is the memory address you want to examine.
(gdb) x/<count><format> <address>

# To view an int value at a specific address, you would use the format specifier /d (for signed decimal)
(gdb) x/d <address>
# /u (for unsigned decimal)
(gdb) x/u <address>

# To view a float value at a specific address, use the /f format specifier:
(gdb) x/f 0x7fffffffde84

# s is a format specifier that tells gdb to interpret the memory at a specified address as a null-terminated string.
(gdb) x/s <address>
(gdb) x/6c <address>
(gdb) x/6c $rsi
# To view an array, you need to specify the number of elements (count) and the format for the type of the elements in the array.
(gdb) x/5d 0x7fffffffdea0


# This examines 4 words of memory starting at the address of x, showing them in hexadecimal.
(gdb) x/4x &x

# 使用 x 命令查看内存内容。x/8xb 表示以 8 进制字节格式显示 8 个字节的内容。
(gdb) x/8xb $rsp       # 显示 rsp 指向的 8 个字节的内容。
# 0x5561dc78:     0x01    0x02    0x03    0x04    0x05    0x06    0x07    0x08
(gdb) x/8xb ($rsp + 8) # 查看 rsp + 8 字节的内存内容

(gdb) x/8xb ($rsp+40)
# 0x5561dca0:     0xc0    0x17    0x40    0x00    0x00    0x00    0x00    0x00

(gdb) x/8xb $rsp 
0x5561dc78:     0xc7    0x44    0x24    0x08    0x39    0x37    0x66    0x61
# This examines 8 bytes of memory starting at the address of $rsp, showing them in hexadecimal.
```
The output shows the contents of memory at address `0x5561dc78` as eight consecutive bytes:
```
0xc7  0x44  0x24  0x08  0x39  0x37  0x66  0x61
```

On an x86-64 system (which is little-endian), memory is stored in little-endian order. This means that:

- The **first byte** (at the lowest address, `0x5561dc78`) is the **least significant byte (LSB)**.
- The **last byte** (at the highest address, `0x5561dc7F`) is the **most significant byte (MSB)**.

### How to Interpret the Order

- **In Memory Order (Little-endian):**
  - Address `0x5561dc78`: 0xc7 (LSB)
  - Address `0x5561dc79`: 0x44
  - Address `0x5561dc7A`: 0x24
  - Address `0x5561dc7B`: 0x08
  - Address `0x5561dc7C`: 0x39
  - Address `0x5561dc7D`: 0x37
  - Address `0x5561dc7E`: 0x66
  - Address `0x5561dc7F`: 0x61 (MSB)

- **As a 64-bit value (big-endian representation):**  
  If you were to interpret these 8 bytes as a single 64-bit integer, because the system is little-endian, the actual numerical value (when viewed in the usual big-endian notation) would be:
  \[
  0x61663739082444c7
  \]
  This is because the bytes are reversed in significance: the byte 0xc7 is the LSB and 0x61 is the MSB.

### Summary

- The bytes are stored in memory in **little-endian order**.
- The order from the lowest to the highest memory address is:  
  **0xc7, 0x44, 0x24, 0x08, 0x39, 0x37, 0x66, 0x61.**
- If you combine them as a 64-bit number, the value is **0x61663739082444c7** (in big-endian representation).

This ordering is standard on x86-64 systems, where the least significant byte is stored at the lowest address.

## 7. View the Call Stack


The call stack shows the sequence of function calls that led to the current point in the program:

```bash
(gdb) backtrace
# This command displays the stack frames, helping you understand how the program arrived at its current state.

# These commands help you track the flow of execution in your program while debugging with gdb.
where: Shows the current call stack and highlights the current line of execution.
frame: Displays the current frame, including the current function and line number.
info line: Provides detailed information about the current line, including memory addresses.
list: Lists the source code around the current line of execution, with the current line highlighted.


```



## 8. Modify Variables

You can modify the value of variables while the program is paused:

```bash
(gdb) set var x = 10
```

This changes the value of x to 10.


## View the machine-level instructions commands

In gdb, the info and disas (short for disassemble) commands are used to gather detailed information about the program you are debugging and to view the machine-level instructions that the CPU executes. 

### Info

The info command is a broad command in gdb that provides information about various aspects of your program's execution, including variables, registers, breakpoints, and more.
info: Provides detailed information about various aspects of the program, such as variables, registers, breakpoints, and more. It has many subcommands to tailor the information you need.


```bash
(gdb) info <subcommand>

# info breakpoints: Lists all the breakpoints, watchpoints, and catchpoints currently set in your program.
(gdb) info breakpoints
(gdb) info b

# info registers: Displays the current contents of all CPU registers.
(gdb) info registers
(gdb) info r
(gdb) i r
(gdb) info registers rdx

rdx    0x7fffffffdde0   140737488344800 
# First Column: Register name (rbx in this case).
# Second Column: Value of the register in hexadecimal format.
# Third Column: Value of the register in decimal format.

# You can use the print (p) command to display the value of a specific register by prefixing it with a dollar sign ($).
(gdb) print $<register_name>

# if %rax contains a memory address, and you want to view the int stored at that address:
(gdb) x/d $rax


# info locals: Shows the values of all local variables in the current stack frame.
(gdb) info locals

# info args: Displays the values of all arguments passed to the current function.
(gdb) info args

# info functions: Lists all the functions that gdb knows about, usually from the current executable and any linked libraries.
(gdb) info functions

# info variables: Lists all global and static variables that gdb knows about.
(gdb) info variables

info stack
# print stack

```


### disas 

The disas (short for disassemble) command in gdb disassembles the machine code of a function or a specific code region, showing the assembly instructions that the CPU executes.
disas: Disassembles machine code, showing the assembly instructions for a function or a specified range of addresses. This is useful for low-level debugging, understanding what the CPU is executing, and inspecting optimized code paths.

```bash
# Disassemble the current function:
(gdb) disas
# This will show the assembly code for the entire function where the current instruction pointer is located.


# Disassemble a specific function:
disas initialize_bomb
# This will disassemble the function my_function and display the corresponding assembly instructions.

# Disassemble a specific range of addresses:
(gdb) disas 0x400610, 0x400650
#This disassembles the code from address 0x400610 to 0x400650.

#Disassemble from the current instruction pointer:
(gdb) disas $pc
# This disassembles a portion of code starting from the current program counter (instruction pointer).
```

# objdump

- Purpose: objdump is a static analysis tool used to display information about binary files. It is commonly used to disassemble executables, object files, or libraries, providing a textual representation of the machine code.
- Functionality: It shows the contents of binary files, such as the assembly instructions, symbol tables, section headers, and more, but does not allow for interactive debugging.
- Non-Interactive: objdump is used to analyze and inspect binaries after they have been compiled, without running them.

## Display the Symbol Table (-t) 

```bash
objdump -t myprogram
```

This will print out the bomb’s symbol table. The symbol table includes the names of all functions and
global variables in the bomb, the names of all the functions the bomb calls, and their addresses. You
may learn something by looking at the function names!


## Disassemble a Binary (-d):

```bash
objdump -d myprogram


# To disassemble the assembly instructions for a specific function using objdump, you can use the -d option along with the --disassemble or --disassemble=<function> flag. Here's how you can do it:
objdump -d --disassemble=my_function myprogram
# Show Source Code with Assembly: If the binary has been compiled with debug symbols (using -g with gcc), you can also see the corresponding source code with -S
```



Use this to disassemble all of the code in the bomb. You can also just look at individual functions.
Reading the assembler code can tell you how the bomb works.
Although objdump -d gives you a lot of information, it doesn’t tell you the whole story. Calls to
system-level functions are displayed in a cryptic form. For example, a call to sscanf might appear
as:
8048c36: e8 99 fc ff ff call 80488d4 <_init+0x1a0>
To determine that the call was to sscanf, you would need to disassemble within gdb.

# strings

This utility will display the printable strings in your bomb.


Looking for a particular tool? How about documentation? Don’t forget, the commands apropos, man,
and info are your friends. In particular, man ascii might come in useful. info gas will give you
more than you ever wanted to know about the GNU Assembler. Also, the web may also be a treasure trove
of information. If you get stumped, feel free to ask your instructor for help.
