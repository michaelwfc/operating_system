#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #include <printUtils.h>

// malloc define a String type
// typedef char String[10];
typedef char *String;

void print_address()
{
    int n = 50;
    printf("\naddress of n=%i is 0x%p\n", n, &n);
    int *p = &n;
    printf("\nvalue in adress 0x%p is %i\n", p, *p);

    // String s = "Hi!";
    // printf("the pointer to string s is %p\n", s);
    // printf("the 0th address %p\n", &s[0]);
    // printf("the 1th address %p\n", &s[1]);
    // printf("the 2th address %p\n", &s[2]);
    // printf("the 3th address %p\n", &s[3]);

    // printf("the 0th char %c\n", *s);
    // printf("the 1th char %c\n", *(s + 1));
    // printf("the 2th char %c\n", *(s + 2));
    // printf("the 3th char %c\n", *(s + 3));
}

void declare_pointer()
{
    int x = 1, y = 2, z[10];
    printf("inital x=%d y=%d z[0]=%d\n", x, y, z[0]);
    int *ip;    /* ip is a pointer to int */
    ip = &x;    /* ip now points to x */
    y = *ip;    /* y is now 1 */
    *ip = 0;    /* x is now 0 */
    ip = &z[0]; /* ip now points to z[0] */
    (*ip)++;
    printf("x=%d y=%d z[0]=%d\n", x, y, z[0]);
}

// void swap(int a, int b)
// // passing by value
// {
//     int temp = a;
//     a=b;
//     b= temp;
// }

void swap(int *px, int *py)
{
    int temp;
    temp = *px;
    *px = *py;
    *py = temp;
}

void pointer_and_array()
{
    int i = 5;
    int a[10];
    int *pa;    // a pointer to an integer
    pa = &a[0]; // same as pa = a; //assign the address of the first element of array a to pa

    a[i] = 99; // same as *(a+i)
    printf("a[%d] = %d\n", i, a[i]);
    printf("*(a+%d) = %d\n", i, *(a + i));
}

/* strlen: return length of string s
'strlen' is declared in header '<string.h>'
*/
int strLength(char *s)
{
    int n;
    for (n = 0; *s != '\0'; s++)
        n++;
    return n;
}

void print_bytes(int *arr, size_t len) {
    unsigned char *p = (unsigned char *)arr;  // interpret memory as bytes
    size_t total_bytes = len * sizeof(int);

    for (size_t i = 0; i < total_bytes; i++) {
        printf("%02X ", p[i]);  // print each byte in hex
        if ((i + 1) % sizeof(int) == 0) {
            printf("| "); // separator between ints
        }
    }
    printf("\n");
}


/**
 * https://pdos.csail.mit.edu/6.828/2019/lec/pointers.c
 */
void pointers_practices(void)
{
    int a[4];   //a points to the start of a stack array
    int *b = malloc(16);  //b points to heap memory from malloc
    int *c;
    int i;

    printf("1: a=%p, b=%p, c=%p\n", a, b, c);

    c = a;
    for (i = 0;i < 4;i++)
    {
        a[i] = 100 + i;
    }
    c[0] = 200;
    printf("2: a[0]=%d, a[1]=%d, a[2]=%d, a[3]=%d\n", a[0], a[1], a[2], a[3]); // 200,101,102,103

    c[1]=300;
    *(c+2)=301;
    3[c]=302;  // same as *(3 + c) = 302, i.e., a[3] = 302, 
    // Fun fact: x[y] is defined as *(x + y) in C, so 3[c] works because it’s *(3 + c).
    printf("3: a[0]=%d, a[1]=%d, a[2]=%d, a[3]=%d\n", a[0], a[1], a[2], a[3]);// 200,300,301,302·

    c= c+1; // c=c+1 is equivalent to c+=1
    *c=400;
    printf("4: a[0]=%d, a[1]=%d, a[2]=%d, a[3]=%d\n", a[0], a[1], a[2], a[3]); // 200,400,301,302


    printf("a address: a: %p, a+1: %p, a+2: %p, a+3: %p\n",a,a+1,a+2,a+3);
    // a address: a: 0x7fffffffd950, a+1: 0x7fffffffd954, a+2: 0x7fffffffd958, a+3: 0x7fffffffd95c
    // Before shift: a=0x7fffffffd950, c=0x7fffffffd954
    // After shift:  a=0x7fffffffd950, c=0x7fffffffd955
    printf("Before shift: a=%p, c=%p\n", (void *)a, (void *)c);
    c= (int *)((char *)c +1);
    printf("After shift:  a=%p, c=%p\n", (void *)a, (void *)c);

    print_bytes(a,4);
     *c =500;
    // (char *)c             : casts it to a char * (pointer to bytes). This changes how pointer arithmetic works:
    // c + 1                 : when c is an int * moves sizeof(int) bytes forward (usually 4 bytes). But (char *)c + 1 moves exactly 1 byte forward.
    //(int *)((char *)c + 1) : Casts the result back to int *. Now c points to an int starting at byte #1 of a[0] — which is unaligned and not the same as a[1].
    // *c = 500;             : Writes the integer 500 (0x000001F4) starting from byte #1. 
    
    
    // little-endian
    // Address:     +0            +1            +2            +3
    //    a:       200           400            301           302
    // Value:   C8 00 00 00 | 90 01 00 00 | 2D 01 00 00 | 2E 01 00 00 |
    // 500 = 0x 00 00 01 F4 -> little-endian -> 0x F4 01 00 00
    // Value:   C8 00 00 00 | 90 F4 01 00 | 00 01 00 00 | 2E 01 00 00 |
    //    a:       200           128144        256           302           
   
    print_bytes(a,4);
    printf("5: a[0]=%d, a[1]=%d, a[2]=%d, a[3]=%d\n", a[0], a[1], a[2], a[3]); 
    
    b= (int *)a +1;
    c= (int *)((char *)a+1);
    printf("6: a=%p, b=%p, c=%p\n",a, b, c);

    // a address: a: 0x7fffffffd950, a+1: 0x7fffffffd954, a+2: 0x7fffffffd958, a+3: 0x7fffffffd95c
    // b:  0x7fffffffd954
    // c:  0x7fffffffd951


}
void main(void)
{
    // print_address();
    // declare_pointer();

    // int x[10];
    // printArray(x, 10);

    // char s[10] = "abc";
    // int n;
    // char s[] = "Hello, world!";
    // char *s = "Hello, world!";
    // n = strLength( s);
    // printf("string of %s lenght= %d",s, n);
    // pointer_and_array();

    pointers_practices();
}
