
# Debug C with WSL gcc  in vscode

## 查看变量
### DEBUG CONSOLE:  直接输入变量名称就可以

```C
// directory 
> var_name

>(char) var_name
// use gdb command, need add -exec
>-exec print var_name

// print address_mask as binary format
-exec print /x address_mask

// run expression 1<<4
>-exec print 1 << 4

// -exec p  getpid(), it shows 'getpid' has unknowun return type, cast the call to its declared return type
-exec p (int)getpid()

//  -exec p (int)waitpid(-1, &status, WUNTRACED), No symbol WUNTRACED in current context
(gdb) define WUNTRACED 0x02
-exec p (int)waitpid(-1, &status, WUNTRACED)

-exec p (int)waitpid(-1, &status, 0x02)
```


## Evaluate macros expression
 GDB cannot evaluate macros directly.

Macros (#define) are processed by the preprocessor, so they don't exist at runtime.

GDB evaluates expressions at runtime, so it cannot directly resolve HDRP(bp).

```gdb

```


# Debug Setting

## launch.json

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug malloc_lab in remote WSL",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/labs/malloc_lab/malloclab-handout/mdriver", // for Ubuntu
      "args": ["-V", "-f", "./traces/amptjp-bal.rep"],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}/labs/malloc_lab/malloclab-handout/",
      "environment": [],
      "externalConsole": false, // This enables an external terminal
      "MIMode": "gdb",
      "miDebuggerPath": "/usr/bin/gdb", // The path to GDB in WSL
      "setupCommands": [
        {
          "description": "Enable pretty-printing for gdb",
          "text": "-enable-pretty-printing",
          "ignoreFailures": false
        }
      ],
      "preLaunchTask": "Build with make mdriver", //Task to compile the code before running the debugger
      "targetArchitecture": "x86_64",
      "logging": { "engineLogging": true }
    },
  ]
}
```

## task.json

use gcc -g to compile the code to allow stop at breakpoints:  
确保您的编译命令（preLaunchTask 中的 Build with make mdriver）生成了带有调试符号的可执行文件。
如果没有启用调试信息（-g），GDB 将无法识别源代码中的断点

```json
{
    "tasks": [
        {
            "label": "Build with make tsh",
            "type": "shell",
            "command": "make",
            "args": [
                "tsh"
            ],
            "options": {
                "cwd": "${workspaceFolder}/labs/shellalb/shlab-handout/"
            },
            "group": "build",
            "problemMatcher": []
        },
    ]
}
```

## c_cpp_properties.json
在 c_cpp_properties.json 文件中添加 includePath 可以帮助 VSCode 的 IntelliSense 正确识别头文件路径，从而提供更好的代码补全和错误检查。虽然 includePath 不会影响编译过程，但它可以显著提升开发体验。