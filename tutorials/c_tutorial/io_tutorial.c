#include <stdio.h>
#include <string.h>

/*
Handling EOF in Different Environments:
On Linux/macOS: Press Ctrl+D to send EOF to the terminal.
On Windows: Press Ctrl+Z followed by Enter to send EOF to the terminal.

scanf() returns the number of items successfully read or EOF when an error occurs or the end of input is reached. You can use it to detect EOF while reading from standard input.
in this example, the program will continue reading integers until Ctrl+D (on Linux/macOS) or Ctrl+Z (on Windows) is pressed, indicating the end of input.
scanf() returns EOF when no more input is available (i.e., when the user sends the EOF signal).

*/
int read_by_scanf()
{
    int num;
    char c;
    printf("Enter numbers (Ctrl+D to end):\n");

    while (scanf("%d %c", &num, &c) != EOF)
    {
        printf("You entered: %d and %c\n", num, c);
    }

    printf("EOF reached, exiting...\n");
    return 0;
}

/*
If you want to read entire lines of text, fgets() is a safer option than scanf(). It reads a line and stops when it encounters a newline character or the end of the file.
in this example, fgets() returns NULL when EOF is reached or when there is an input error.
fgets() reads one line at a time, so it's good for processing input line-by-line.

If fgets() successfully reads a line, it returns the pointer to the cmdline buffer.
If an error occurs or the end of the file (EOF) is reached, it returns NULL.


Why Not Just fgets() Check Alone?
Using fgets(cmdline, MAXLINE, stdin) == NULL by itself could be misleading because fgets() returns NULL in two cases:

End-of-File (EOF): No more data to read, which isn't an error.
Error while reading: This is the actual problem we want to catch.
By combining the fgets() check with ferror(stdin), we can specifically handle the error case (where fgets() failed due to an I/O error) and ignore the EOF case.

The newline character ('\n') that the user enters by pressing Enter is stored in the buffer. However, the behavior depends on how much data is read and the buffer size.

The fgets() function reads characters from the standard input (stdin) into a provided buffer, stopping under the following conditions:

It reads up to n - 1 characters (where n is the size of the buffer) or until a newline character ('\n') is encountered.
It always null-terminates the string with a '\0' character, regardless of how much data is read.
If a newline character ('\n') is encountered before the buffer is filled, the newline is included in the buffer.
If the buffer is large enough to hold the entire input, the newline character will be included, and the function will return the complete string (including the newline).


*/
int read_by_fgets()
{
    char buffer[100];
    int max_size = 10;

    printf("Enter lines (Ctrl+D to end):\n");

    while (fgets(buffer, max_size, stdin) != NULL)
    {
        printf("You entered: %s", buffer);
    }

    printf("EOF reached, exiting...\n");
    return 0;
}

/*
If you want to read characters one by one, you can use getchar() which returns EOF when the end of input is reached.
getchar() returns the next character from stdin, and when the EOF is encountered, it returns EOF.
*/

int read_by_getchar()
{
    int ch;

    printf("Enter characters (Ctrl+D to end):\n");

    while ((ch = getchar()) != EOF)
    {
        putchar(ch);
    }
    
    printf("\nEOF reached, exiting...\n");
    return 0;
}

/*
Buffered Output in C
In C, the standard output (stdout) is typically line-buffered or fully buffered, meaning that the output you print with printf() isn't necessarily immediately written to the terminal. Instead, it's stored in a temporary memory area called the output buffer. The contents of this buffer are only written to the terminal (or the file) under certain conditions:

If the buffer is full.
If a newline character (\n) is encountered (in the case of line-buffered output).
When the program terminates (for standard output in most environments).
If you explicitly flush the buffer.

*/

void printf_with_fflush()
{   
    int MAX_BUF_SIZE =50;
    char name_buffer[50];

    while (1)
    {
        printf("Enter your name: "); // Prompt to ask user for input
        fflush(stdout);              // Make sure the prompt appears before waiting for input
        /*
        Using fgets(cmdline, MAXLINE, stdin) == NULL by itself could be misleading because fgets() returns NULL in two cases:
        - End-of-File (EOF): No more data to read, which isn't an error.
        - Error while reading: This is the actual problem we want to catch.
        By combining the fgets() check with ferror(stdin), we can specifically handle the error case (where fgets() failed due to an I/O error) and ignore the EOF case.
        */
        if ((fgets(name_buffer, sizeof(name_buffer), stdin) == NULL) && ferror(stdin))
        {
            fprintf(stderr, "Error reading input.\n");
        };
        name_buffer[strlen(name_buffer)-1] = '\0'; /* replace trailing '\n' with null terminator */
        // printf("You entered: %s,string lenth:%ld", name_buffer,strlen(name_buffer));
        printf("Hello, %s!\n", name_buffer);
    }
}

void main()
{
    //  read_by_scanf();
    //  read_by_fgets();
    printf_with_fflush();
}
