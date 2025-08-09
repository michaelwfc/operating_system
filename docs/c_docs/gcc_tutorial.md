# Basic gcc compile

To compile a C file using `gcc`, you can use the following basic command:

```bash
gcc filename.c -o outputfile

# 包含头文件路径： if filename.c 依赖于 csapp.h 头文件，并且该头文件不在标准路径中，假设 csapp.h 位于 /mnt/e/projects/csapp/include/ 目录下， 你需要使用 -I 标志指定头文件路径。
gcc filename.c -o outputfile -I/path/to/csapp/include/ 

# 链接外部库： 如果 hostinfo.c 依赖于某些外部库（例如 libcsapp），你需要使用 -l 标志链接这些库，并使用 -L 标志指定库的路径。假设 libcsapp 位于 /mnt/e/projects/csapp/lib/ 目录下，可以这样编译：
gcc filename.c -o outputfile -I/mnt/e/projects/csapp/include/ -L/mnt/e/projects/csapp/lib/ -lcsapp


gcc hostinfo.c csapp.c -o hostinfo2 -lpthread -lrt

```

### Explanation:
- `gcc`: The GNU Compiler Collection, which compiles C programs.
- `filename.c`: The C source file you want to compile.
- `-o outputfile`: Specifies the name of the output file (the compiled executable). If you omit this part, `gcc` will default to producing an executable called `a.out`.

### Example:
If you have a C source file named `program.c`, you can compile it like this:

```bash
gcc program.c -o program
```

This will compile `program.c` and produce an executable named `program`.

## To Compile Multiple Files:
If your program consists of multiple C source files, you can specify them all in the command:

```bash
gcc file1.c file2.c -o program
```

This will compile `file1.c` and `file2.c` together and link them into a single executable named `program`.

## To Link External Libraries:
If your program needs external libraries, such as the math library, you can specify them with the `-l` flag:

```bash
gcc program.c -o program -lm
```
In this example, `-lm` links the math library, which is often necessary when using functions like `sqrt()` or `sin()`.



# gcc flags

Here is a table listing the most common `gcc` flags along with their descriptions:

| **Flag**                | **Description**                                                                 |
|-------------------------|---------------------------------------------------------------------------------|
| `-o <outputfile>`        | Specifies the name of the output file(executable),the defalut is a.out         |
| `-g`                     | Includes debugging information in the compiled，Without debugging symbols, GDB might not have full access to the types and constants used in your program.|
| `-Wall`                  | Enables most warning messages (highly recommended).                           |
| `-Wextra`                | Enables additional warning messages beyond `-Wall`.                           |
| `-Werror`                | Treats all warnings as errors (prevents compilation if there are any warnings).|
| `-std=gnu99`                | By default, the gcc compiler uses the latest C standard (e.g., C11 or C17), Specifies the C language standard to use. `gnu99` is the GNU extension of the C99 standard. It allows additional features and extensions, like inline assembly and certain built-in functions, not available in strict C99. |
| `-O0`                    | Disables optimization (useful for debugging).                                 |
| `-O1`, `-O2`, `-O3`      | Different levels of optimization (higher values increase optimization).       Optimization Flag: Avoid using optimization flags (such as -O2) when compiling your program for debugging, as optimization can make debugging difficult by reordering or removing code. Stick to -g for debugging purposes      |
| `-U_FORTIFY_SOURCE`         | Unsets the `_FORTIFY_SOURCE` macro, which controls certain optimizations that can improve security for certain functions (like `memcpy`, `printf`, etc.) by adding bounds checks. Unsetting it disables these checks. |
| `-D_FORTIFY_SOURCE`         | Defines the `_FORTIFY_SOURCE` macro. It enables various compiler-level optimizations to enhance security by performing bounds checking on certain standard library functions. A common value for this is `2` for stricter checks. |
| `-D__USE_XOPEN_EXTENDED`    | Defines the `__USE_XOPEN_EXTENDED` macro, enabling certain POSIX extensions that provide additional system calls and functions beyond the basic POSIX specification (e.g., `strptime`, `strtok_r`, etc.). |
| `-c`                     | Compiles the source file(s) into object file(s) but does not link.             |
| `-S`                     | Compiles the source code to assembly code but does not assemble or link.       |
| `-E`                     | Preprocesses the source code only, does not compile or assemble.               |
| `-l<library>`            | Links with the specified library (e.g., `-lm` for the math library).           |
| `-L<path>`               | Adds a directory to the library search path.                                   |
| `-Ofast`                 | Enables aggressive optimizations that may violate strict standard compliance.  |
| `-std=<standard>`        | Specifies the C language standard to use (`c99`, `c11`, `gnu99`, etc.).       |
| `-ansi`                  | Enforces strict ANSI C compliance (prevents using GNU extensions).            |
| `-pedantic`              | Enforces standard compliance and shows all warnings related to non-compliant code. |
| `-Wno-<warning>`         | Suppresses a specific warning (e.g., `-Wno-unused-variable`).                |
| `-Wundef`                | Warns if an undefined macro is used.                                           |
| `-Wunused`               | Warns about unused variables, functions, etc.                                 |
| `-D<macro>`              | Defines a preprocessor macro.                                                 |
| `-I<directory>`          | Adds a directory to the include search path for header files.                 |
| `-fno-<option>`          | Disables a specific optimization or feature (e.g., `-fno-strict-aliasing`).   |
| `-v`                     | Shows detailed information about the compilation process.                     |
| `-M`                     | Generates a makefile with dependencies based on the source file.               |
| `-pipe`                  | Uses pipes rather than temporary files for communication between stages.      |
| `-static`                | Links the program statically (no dynamic libraries).                          |
| `-shared`                | Creates a shared library.                                                     |
| `-fPIC`                  | Generates position-independent code (necessary for shared libraries).         |
| `-fopenmp`               | Enables OpenMP parallel programming support.                                  |
| `-march=native`          | Optimizes code for the architecture of the current machine.                   |

This table includes some of the most commonly used flags with `gcc`, but there are many other specialized flags for various use cases. You can always refer to the `gcc` documentation (`man gcc` or `gcc --help`) for a comprehensive list of available flags.
