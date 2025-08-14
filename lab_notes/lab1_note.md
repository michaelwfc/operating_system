# Labs code
- https://github.com/PKUFlyingPig/MIT6.S081-2020fall.git
- https://pdos.csail.mit.edu/6.828/2021/labs/util.html

```bash
git clone https://github.com/PKUFlyingPig/MIT6.S081-2020fall.git
# $ git clone git://g.csail.mit.edu/xv6-labs-2021
# Cloning into 'xv6-labs-2021'...
...
$ cd xv6-labs-2021
$ git checkout util
# Branch 'util' set up to track remote branch 'util' from 'origin'.
# Switched to a new branch 'util'

# 给 python 建一个指向 python3 的符号链接
sudo ln -s $(which python3) /usr/local/bin/python


```
# 1. sleep

```bash
make qemu
sleep
# Your solution is correct if your program pauses when run as shown above. Run make grade to see if you indeed pass the sleep tests.



# in Ubutnu bash
./grade-lab-util sleep
# == Test sleep, no arguments == 
# sleep, no arguments: OK (3.5s) 
# == Test sleep, returns == sleep, returns: OK (0.4s) 
# == Test sleep, makes syscall == sleep, makes syscall: OK (1.0s) 
```

## Files / functions to know (xv6-riscv)

* **User side**

  * `user/usys.pl` → generates syscall stubs (creates `usys.S` / `usys.o`).
  * `user/usys.S` (generated): tiny assembly stubs that arrange syscall number and issue `ecall`.
  * `user/ulib.c`: may contain convenient wrappers (in some xv6 versions). A user program typically calls `sleep()` which resolves to the generated stub.

* **Kernel side**

  * `kernel/trampoline.S` — user ↔ kernel transition code (saves/restores registers, switches to kernel page table).
  * `kernel/trap.c` — `usertrap()` (entry for traps from user mode) and `usertrapret()` (return to user).
  * `kernel/syscall.c` — `syscall()` dispatcher (reads syscall number and calls handler).
  * `kernel/sysproc.c` — implementation of `sys_sleep()` (the syscall handler).
  * `kernel/proc.c` — implementation of the internal `sleep(chan, lock)` used by the kernel to block processes.
  * `include/syscall.h` — syscall numbers (e.g. `SYS_sleep`).
  * `struct trapframe` (defined in `kernel/trap.h` or `kernel/proc.h`) — kernel-side saved user registers (a0-a7 are stored here).

---

## Quick overview (one-line)

User calls `sleep(n)` → user syscall *stub* issues `ecall` → trap into kernel (trampoline → `usertrap`) → kernel `syscall()` dispatcher reads syscall number (a7) → calls `sys_sleep()` → `sys_sleep()` uses kernel `sleep(&ticks, &tickslock)` (internal scheduler sleep) → returns → kernel writes return value into trapframe (a0) → `usertrapret` / trampoline returns to user.



1. User-space sleep function definition:
The sleep function callable from user programs is defined in user/user.h (as referenced in your code) and user/usys.pl (which generates syscall stubs)
The actual implementation that user code links to is in user/usys.S, which contains the assembly code to make the system call, 
that jumps from user code into the kernel for sleep.
In user/ulib.c, sleep just calls the sleep system call via a generated stub.
Example in user/usys.pl:
entry("sleep");
This creates the actual assembly stub that invokes the SYS_sleep number.

2. Kernel implementation:
The kernel-side implementation of the sleep system call is: sys_sleep() in kernel/sysproc.c,
The system call table that maps the user call to kernel function is in kernel/syscall.c.

3. Library function:
The atoi function you're using is implemented in user/ulib.c.


Nice — here’s a clear, step-by-step map of how the **`sleep`** *system call* in **xv6-riscv** flows from a user program into the kernel and back, plus where the relevant code lives and how you can observe/debug it.

##  Detailed step-by-step flow

1. **User code calls `sleep(n)`**

   * The user program calls `sleep(n)`. That symbol is implemented by an assembly stub (generated from `usys.pl`) or wrapper that prepares the call.

2. **User syscall stub (assembly)**

   * The stub puts the syscall number (e.g. `SYS_sleep`) into register **a7** and syscall args into **a0, a1, ...**, then executes the RISC-V `ecall` instruction.
   * (This stub lives in `user/usys.S` or the file generated from `usys.pl`.)

3. **Trap to kernel via trampoline**

   * `ecall` from U-mode triggers a trap; hardware transfers control into the **trampoline** code (`trampoline.S`), which:

     * Switches to kernel page table,
     * Saves user registers into the process `trapframe`,
     * Jumps to the kernel handler `usertrap()` defined in `kernel/trap.c`.

4. **`usertrap()` inspects the cause**

   * `usertrap()` checks `scause` to see what caused the trap.
   * If the cause is an environment call (`ecall`) from user mode, `usertrap()` will call the syscall dispatcher: `syscall()`.

5. **`syscall()` dispatcher (kernel/syscall.c)**

   * `syscall()` reads the syscall number from `p->trapframe->a7` (where `p` is the current `struct proc *`).
   * It uses the syscall number to index into a syscall function table (array `syscalls[]`) and calls the corresponding `sys_*` function (here `sys_sleep()`), passing through arguments as needed (via helper functions like `argint()`).

6. **`sys_sleep()` (kernel/sysproc.c)**

   * `sys_sleep()` retrieves the integer argument `n` (the number of ticks) using `argint(0, &n)` which reads from the saved trapframe (a0) or stack as appropriate.
   * It then acquires the `tickslock`, records the current `ticks`, and loops:

     ```c
     while(ticks - ticks0 < n){
       if(killed(myproc())) { release(...); return -1; }
       sleep(&ticks, &tickslock);
     }
     ```
   * **Important:** `sleep(chan, lock)` here is *kernel internal* — it puts the calling process to sleep (blocks it) on the `chan` until `wakeup(chan)` is called. It expects you to hold `tickslock` when calling and will release it internally while blocking.

7. **When the sleep completes**

   * `sys_sleep()` releases locks and returns `0` (or -1 if interrupted). The return value is stored into the process `trapframe->a0`.

8. **Return to user**

   * `usertrap()` finishes, and `usertrapret()` (using `trampoline.S`) restores user registers from the `trapframe` and executes `sret`, returning execution back to user mode. The user program now resumes, seeing the return value in its `sleep()` call.

---


# 2. pingpong

```bash
make qemu
pingpong
# 12216: received ping
# 12208: received pong

# in ubuntu bash
./grade-lab-util pingpong
# make: 'kernel/kernel' is up to date.
# == Test pingpong == pingpong: OK (0.9s)

```

# 3. primes
- https://swtch.com/~rsc/thread/
  
## goal

Write a concurrent version of prime sieve using pipes. This idea is due to Doug McIlroy, inventor of Unix pipes. The picture halfway down this page and the surrounding text explain how to do it. Your solution should be in the file user/primes.c.

Your goal is to use pipe and fork to set up the pipeline. 
- The first process feeds the numbers 2 through 35 into the pipeline. 
- For each prime number, you will arrange to create one process that reads from its left neighbor over a pipe and writes to its right neighbor over another pipe. 
- Since xv6 has limited number of file descriptors and processes, the first process can stop at 35.
  
## 0. Sieve of Eratosthenes


The **Sieve of Eratosthenes** is a classic and efficient algorithm for finding all prime numbers up to a given limit $n$.
It works by progressively eliminating the multiples of each prime number starting from 2.

---

### **How it works step-by-step**

Let’s say we want all primes ≤ $n$:

1. **Create a list** of integers from $2$ to $n$ (initially assume all are prime).
2. **Start with the first prime**: $p = 2$.
3. **Eliminate multiples of $p$**: mark $2p, 3p, 4p, \dots$ as not prime.
4. **Find the next number** in the list that’s still marked as prime — that’s your next $p$.
5. Repeat steps 3–4 until $p^2 > n$ (because any composite number ≤ $n$ will have a factor ≤ $\sqrt{n}$).
6. **Remaining unmarked numbers are all primes**.

---

### **Example: $n = 30$**

Numbers:
`[2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30]`

* Start with **2** → cross out multiples: 4, 6, 8, …
* Next prime is **3** → cross out 6, 9, 12, …
* Next prime is **5** → cross out 10, 15, 20, …
* Next prime is **7**, but $7^2 = 49 > 30$ → stop.

Remaining primes:
**2, 3, 5, 7, 11, 13, 17, 19, 23, 29**

---

### **Time Complexity**

* **O(n log log n)** — much faster than checking each number for primality individually.

---

If you like, I can **draw you a visual table** showing how numbers get crossed out step-by-step — it makes the sieve idea very intuitive. Would you like me to do that?


Ah — this is the **primes pipeline** problem from MIT 6.S081’s lab.
The reason we use a **pipe** here is not just to pass numbers, but to **model the sieve algorithm** as a *chain of cooperating processes*.

Let me break it down:

---

## 1. What we’re trying to simulate

We want to implement the **Sieve of Eratosthenes**, but instead of doing it in one process with loops and arrays, we do it *concurrently* using **multiple processes** connected by pipes.

* **Process 1**: sends numbers `2..35` into the pipeline.
* **Process 2**: reads the first number it sees (say, `2`) → prints it → starts a new process that filters out all multiples of `2`.
* The **next process**: reads the next prime (`3`) → prints it → starts another filter process, and so on.

---

## 2. Why we need a pipe

Without pipes, each process would need to:

* Share some memory structure (but xv6 doesn’t have shared memory between processes by default).
* Or read from a file (slow and cumbersome).

A **pipe** is:

* A unidirectional communication channel between processes.
* Lets us *stream* numbers from one process to the next without storing them all at once.
* Naturally models “neighbor-to-neighbor” communication in the sieve.

Think of it like **passing a stream of candidates down a conveyor belt** — each worker (process) takes the first item it sees as its prime, removes all its multiples, and passes the rest on.

---

## 3. How it looks in action

If we run the program, here’s the flow:

```
[Process feeding 2..35] -> (pipe) -> [prime=2 filter] -> (pipe) -> [prime=3 filter] -> ...
```


1. main process:
- create the first pipe pipe1
- writes 2..35 into pipe1.
- Closes the write end and forks the first prime filter process.

2. Prime filter process
- Reads the first number from the pipe → this is the prime.
- Prints it.
- Creates a new pipe for the next process.
- Filters out numbers divisible by this prime and writes the rest into the new pipe.
- Forks the next prime filter process and passes the new pipe to it.
- This repeats until no more numbers remain.


---




## 4. Why this is important in xv6’s lab

This problem forces you to:

* Use `pipe()` and `fork()` to chain processes.
* Handle file descriptor closing correctly (otherwise you get deadlocks).
* Understand how processes in xv6 communicate without shared memory.
* Practice **process lifetime management** (each filter process eventually exits).

---


```bash
make qemu
primes
# prime 2
# prime 3
# prime 5
# prime 7
# prime 11
# prime 13
# prime 17
# prime 19
# prime 23
# prime 29
# prime 31


./grade-lab-util primes
# make: 'kernel/kernel' is up to date.
# == Test primes == primes: OK (0.9s) 

```

# 4. find
```bash
make qemu

find ls
#./ls


./grade-lab-util find
# make: 'kernel/kernel' is up to date.
# == Test find, in current directory == find, in current directory: OK (1.3s) 
# == Test find, recursive == find, recursive: OK (1.1s) 
```

# 5. xargs

xargs is basically a command argument builder — it takes data from standard input (stdin) and turns it into arguments for another command.
stdin (text) → split into arguments → run command with those arguments

### Why it’s useful
- Many commands output lists of items (filenames, words, numbers).
- Some other commands need those items as arguments, not from stdin.
- xargs is the bridge between the two.


xargs splits input by whitespace/newlines (unless you tell it otherwise).
```bash
$ echo  "1\n2" | xargs  echo line
# $ echo -e "1\n2" | xargs -n 1 echo line
line 1
line 2
```



## Some hints:

- Use fork and exec to invoke the command on each line of input. Use wait in the parent to wait for the child to complete the command.
- To read individual lines of input, read a character at a time until a newline ('\n') appears.
- kernel/param.h declares MAXARG, which may be useful if you need to declare an argv array.
- Add the program to UPROGS in Makefile.
- Changes to the file system persist across runs of qemu; to get a clean file system run make clean and then make qemu.

xargs, find, and grep combine well:
```bash
$ find . b | xargs grep hello

# xargs: This command takes the list of file paths piped from find and converts them into arguments for another command. It's useful for executing a command once for many files.
# grep hello: This is the command that xargs runs. grep is a tool for searching text. It will search for the pattern "hello" inside every file path it receives from xargs

```
will run "grep hello" on each file named b in the directories below ".".


## test
To test your solution for xargs, run the shell script xargstest.sh. Your solution is correct if it produces the following output:

### xargstest.sh
```bash
$ cat xargstest.sh
mkdir a
echo hello > a/b
mkdir c
echo hello > c/b
echo hello > b
find . b | xargs grep hello


```

### How It Works Together
- find generates a list of every item in the current directory and the b directory.
  The output of find .b is a single string where each found path is separated by a newline character (\n).

.b/
├── file1.txt
└── subdir/
    └── file2.log

The output of find .b is a single string where each found path is separated by a newline character (\n).

It is not a char** array. That data type is specific to programming languages like C/C++. Shell commands print their results to a standard output stream (stdout), which is fundamentally a stream of bytes.

## How It Works

1. find . b Creates the Text Stream
The find command runs and outputs a single stream of text to "standard output" (stdout). Each path it finds is separated by a newline character (\n).

Example
Imagine you have this directory structure:

./
├── a.txt   (contains the text "hello world")
└── b/
    └── c.txt   (contains "say hello")

The raw stream of text produced would look like this (the order might vary):

./a.txt\n./b\n./b/c.txt\n


This stream is now heading towards the pipe (|).

2. The Pipe (|) Redirects the Stream
The pipe acts like a hose. It takes the entire text stream coming out of find and redirects it to become the "standard input" (stdin) for the xargs command. xargs is now ready to read that stream.

3. xargs Converts the Stream into Arguments
xargs is a utility that reads text from stdin and converts it into command-line arguments. By default, it uses newlines (and other whitespace) to separate the items.

It reads the stream ./a.txt\n./b\n./b/c.txt\n... and effectively says, "Okay, I have a list of things: ./a.txt, ./b, ./b/c.txt, etc. I will now run grep hello and append this list to it."

It constructs and executes the following command:
```bash
grep hello ./a.txt ./b ./b/c.txt b c.txt
```

4. grep Searches the Files
Finally, the grep command receives the list of file and directory paths. It goes through them one by one and performs its search for the word "hello".

It searches ./a.txt and finds a match.
It tries to search the directory ./b and prints an error like grep: ./b: Is a directory.
It searches ./b/c.txt and finds a match.
...and so on for the other paths.

The final output you see on your screen is only from grep. It will be the successful matches and any errors from trying to search directories.


A common issue with the command as written is that find also lists directories themselves (e.g., ./b), which grep cannot search, potentially leading to error messages like grep: b: Is a directory.


```bash
$ make qemu
...
init: starting sh
# Redirect standard input (stdin) from the file xargstest.sh
$ sh < xargstest.sh
$ $ $ $ $ $ hello
hello
hello
$ $   
```

You may have to go back and fix bugs in your find program. The output has many \$ because the xv6 shell doesn't realize it is processing commands from a file instead of from the console, and prints a $ for each command in the file.


