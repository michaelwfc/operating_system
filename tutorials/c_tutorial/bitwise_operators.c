#include <stdio.h>
#include <limits.h>

/*
The char type is typically a signed 8-bit integer, which means it can store values from -128 to 127.
The unsigned char type is an 8-bit integer that can store values from 0 to 255.

The int type is typically a signed 32-bit integer, which means it can store values from -2,147,483,648 to 2,147,483,647.

*/
void get_dtypes_range()
{

    printf("Signed char range: %d to %d\n", SCHAR_MIN, SCHAR_MAX);
    printf("Unsigned char range: %u to %u\n", 0, UCHAR_MAX);

    printf("Signed int range: %d to %d\n", INT_MIN, INT_MAX);
    printf("Unsigned int range: %u to %u\n", 0, UINT_MAX);

    int char_byte_size;
    char_byte_size = sizeof(char);
    printf("char_byte_size is: %d\n", char_byte_size);

    int int_byte_size = sizeof(int);
    printf("int_byte_size is: %d\n",int_byte_size);

}

/*
If	there	is	a	mix	of	unsigned	and	signed	in	single	expression, signed	values	implicitly	cast	to	unsigned	
Including	comparison	opera;ons	<,	>,	==,	<=,	>=

2147483647U = 0111,1111,FF,FF,FF   = 2147483647
2147483648U = 1000,0000,00,00,00   = 2147483648
(int)2147483648U= 1000,0000,00,00,00 = -2147483648
*/
void dataTypeCasting()
{
    int r;
    r = (int) 2147483648U;// 
    printf("INT_MAX+1: 2147483648U  when casting to int(signed) = %d\n", r);


    // Demo_4
    /*
    signed i - unsigned sizeof(char)  -> unsigned i - unsigned size= 
    */
    int n = 10, i; 
    for (i = n - 1 ; i - sizeof(char) >= 0; i--)
        printf("i: 0x%x\n",i);

    if (-1 > 0U)                     // 神奇的算术!! 
        printf("You Surprised me!\n"); 

}


void printChar(char ch)
{
    // Use printf with %c format specifier to print the character
    printf("The character is: %c\n", ch);

    printf("The interger for character %c is: %d\n", ch,ch);
    

    // Cast the character to an integer to get its ASCII value
    // Although casting is not strictly necessary, it makes the intention clear
    int asciiValue = (int) ch;

    // Print the character and its ASCII value
    printf("The ASCII value of %c is %d\n", ch, asciiValue);

    printBinary(ch);

    printf("The hexadecimal value of '%c' is: 0x%X\n", ch, ch); // Print the character in uppercase hexadecimal
}


// Function to print the binary representation of an integer
void printBinary(char ch) {
    printf("The binary representation for char %c is:", ch);
    // Create a mask with a 1 in the most significant bit position
    // For a 32-bit int, this would be 10000000000000000000000000000000 (0x80000000)
    unsigned int mask = 1 << (sizeof(char) * 8 - 1);

    // Iterate through each bit of the integer
    for (int i = 0; i < sizeof(char) * 8; i++) {
        // Use bitwise AND to check if the current bit is 1 or 0
        if (ch & mask) {
            // If the result is non-zero, the current bit is 1
            printf("1");
        } else {
            // If the result is zero, the current bit is 0
            printf("0");
        }
        // Shift the mask right by 1 to check the next bit in the next iteration
        mask >>= 1;
    }
    // Print a newline character after printing the binary representation
    printf("\n");
}


/* getbits: the (right adjusted) n-bit field of x that begins at position p.
The unsigned keyword by itself is shorthand for unsigned int.
*/
unsigned getbits(unsigned x, int p, int n)
{
return (x >> (p+1-n)) & ~(~0 << n);
}


void printCharOverflow()
{
    char a=127;
    char b=a+1; // -128
    printf("%d + %d = %d\n", a,1,b);
    char c = a+2;  // -127
    printf("%d + %d = %d\n", a,2,c);

    char a1 = -1;

}

typedef unsigned char *pointer;

void show_bytes(pointer start, size_t len){
    size_t i;
    for (i = 0; i < len; i++)
        printf("%p\t0x%.2x\n",start+i, start[i]);
    printf("\n");
}




void main(void)
{   
    // get_dtypes_range();
    // dataTypeCasting();

    // char ch = 'A';
    // printChar(ch);
    
    // char ch = 122;
    // printBinary(ch);

    // char ch2 = 177;
    // printBinary(ch2);

    int a= - 15213;
    printf("int a = %d , hex: %x;\n",a,a);
    show_bytes((pointer) &a, sizeof(int));
   





}