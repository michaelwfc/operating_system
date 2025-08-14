#include <stdio.h>
#include <string.h>
// #include <utils.h>

int get_int(void);


void meow(int n);
int add(int x, int y);
int* get_array(int n);
void printArray(int* arr,int length);
int sizeof_array(int []);
// C Program to demonstrate usage of macros to find the size of arrays
// #define sizeof_array(arr) sizeof(arr) / sizeof(*arr)

float average(int array[]);
const char* get_string(void);
// malloc define a String type
typedef char String[10];

int length_of_string(String string);




 int main(void)
 {
    printf("Welcome to C World\n");

   //  meow(3);
   //  char  name[] = "michael";
   
   // char name[50];
   // printf("Please input your name: ");
   // scanf("%s", name);
   // printf("Hello, %s\n", name);

   // printf("Please input your name: ")
   // const char* pname = get_string();
   // printf("Hello, %s\n", pname);

   // printf("Input X:")
   // int x = get_int();
   // printf("Input Y:")
   // int y = get_int();
   // if (x<y)
   // {printf("x is less than y\n");
   // };

   // int result = add(x,y);
   // printf("x + y = %i\n",result);

   // int arr[] = {1,2,3};
   // int n = (&arr)[1]-arr;
   // int length = sizeof_array(arr);
   // printArray(arr,length);


   // int* ptr = &arr[0];
   // printf("Address Stored in Array name: %p\n
   //         "Address of 1st Array Element: %p\n",
   //         arr, &arr[0]);
   
   // print the value of point
   // printf("value at pointer %p is %d\n",ptr,*ptr);
   // printf("value at next pointer %p is %d\n",ptr+1,*(ptr+1));

   // printing array elements using pointers
   // printf("Array elements using pointer: ");
   // for (int i = 0; i < 5; i++) {
   //      printf("%d ", *ptr++);
   // }
   // printf("\n");

   
   // int* arr2 = get_array(length);
   // printArray(arr2,length);

   // float ave =  average(arr);
   // printf("average = %f\n",ave);
   char s1[] = "hello";
   String s= "Hi!";
   printf("%s\n",s);
   printf("%i %i %i\n",s[0],s[1],s[2]);
   int n = length_of_string(s);
   // int n = strlen(s);
   printf("length of %s is %d\n",s,n);


 }


 void meow(int n)
{  
   for(int i=0;i<n;i++)
   {
      printf("meow\n");
   }
}


int add(int x, int y)
{
   return x+y;
}





int get_int(void)
{
   // char greet[],int index
   // printf(greet,index);
   int num;
   scanf("%i", &num);
   
   return num;

}


int* get_array(int n)
/*C Program to return array from a function:
 In C, we can only return a single value from a function. To return multiple values or elements, 
 we have to use pointers. We can return an array from a function using a pointer to the first element of that array.
 
 Note: You may have noticed that we declared static array using static keyword. 
 This is due to the fact that when a function returns a value, all the local variables and other entities 
 declared inside that function are deleted. So, if we create a local array instead of static, 
 we will get segmentation fault while trying to access the array in the main function.
*/
{  
   // const int  N =3;
   static int array[3];
   for(int i=0; i<n;i++)
   {  
      char greet[] = "please enter the int of arry[%d]:";
      printf(greet, i);
      int num = get_int();
      array[i]=num;
   }
   printf("Size of array = %lu\n",sizeof(array));
   return array;
}


void printArray(int* arr,int length)
// An array is always passed as pointers to a function in C. Whenever we try to pass an array to a function,
// it decays to the pointer and then passed as a pointer to the first element of an array.
// https://www.geeksforgeeks.org/pass-array-value-c/?ref=lbp
{
   //  printf("Size of Array in Functions: %lu\n", sizeof_array(arr));
   //  printf("Array Elements: ");
   // int length =  sizeof_array(arr);

   for (int i = 0; i < length; i++) {
        printf("%d ",arr[i]);
    }
    printf("\n");
}

int sizeof_array(int array[])
/*
Reference: https://www.geeksforgeeks.org/pointer-vs-array-in-c/?ref=lbp
1. the sizeof operator
sizeof(array) returns the amount of memory used by all elements in the array 
sizeof(pointer) only returns the amount of memory used by the pointer variable itself 

2. the & operator 
array is an alias for &array[0] and returns the address of the first element in the array 
&pointer returns the address of the pointer

*/
{  
   int length = sizeof(array)/sizeof(array[0]);
   // int length = (&array)[1]-array; 

   return length;
}


int length_of_string(char s[])
/*
Array members are accessed using pointer arithmetic
The compiler uses pointer arithmetic to access the array elements. 
For example, an expression like “arr[i]” is treated as *(arr + i) by the compiler. 
That is why the expressions like *(arr + i) work for array arr, and expressions like ptr[i] 
also work for pointer ptr.
*/
{
   int n = 0 ;
   while (s[n] != "\0")
   {
      n++;
   }
   return n;
}


float average(int array[])
{  
   float sum=0;
   // int length =  *(&array+1) -array;
   int length =  sizeof_array(array);
   for (int i=0;i<length;i++)
   {
      sum += array[i];
   }

   float result = sum/length;
   return result;
}


const char* get_string(void )
/*
   1. Strings in C are arrays of char elements, so we can’t really return a string - 
   we must return a pointer to the first element of the string.
   2. you can’t return a string defined as a local variable from a C function, 
   because the variable will be automatically destroyed (released) when the function finished execution, 
   and as such it will not be available
*/
{
   // printf("%s",greet);
   //using statically allocated strings:: static does here is that the strings get put into the data segment of the program. 
   //That is, it's permanently allocated.
   static char name[50];
   scanf("%s", name);
   // a pointer to a array (string is a array of char)
   // char* pname = name;
   return name;
}


