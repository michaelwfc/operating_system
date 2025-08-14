#include <stdio.h>
#include <stdlib.h> // For malloc and free

/*
malloc: 
Allocates a block of memory of a specified size but does not initialize the memory. 
The allocated memory contains garbage values (whatever was in that memory previously).

void* malloc(size_t size);
int *arr = (int *)malloc(5 * sizeof(int));  // Allocates memory for 5 integers

- Takes a single argument, which is the total size in bytes to allocate.



In C, malloc (memory allocation) is a function used to allocate a block of memory on the heap dynamically. 
This means the size of the memory can be determined during runtime, which is useful when the size of the data is not known at compile time.

Why Use malloc?
Dynamic Memory Allocation: Useful when the required memory size is not known beforehand.
Heap Memory: Allocates memory on the heap, which persists until explicitly freed using free.
Flexibility: Allows creating  dynamic  data structures like linked lists, trees, and dynamic arrays where the size can change during the program execution.



calloc:
Allocates memory for an array of elements and initializes all the memory to zero. It ensures that the allocated memory starts with zeroed-out bytes.

void *calloc(size_t num_elements, size_t element_size);
int *arr = (int *)calloc(5, sizeof(int));  // Allocates memory for 5 integers and initializes them to 0


- num_elements: The number of elements to allocate memory for.
- element_size: The size of each element in bytes.




*/
int main()
{
    int *array;
    int n, i;

    printf("Enter the number of elements: ");
    scanf("%d", &n);

    // Allocate memory for n integers
    array = (int *)malloc(n * sizeof(int)); // (int *) is used to cast the void pointer returned by malloc to an integer pointer.
    if (array == NULL) // Check if malloc succeeded
    {
        printf("Memory allocation failed\n");
        return 1;
    }

    // Initialize array
    for (i = 0; i < n; i++)
    {
        array[i] = i + 1;
    }

    // Print the array elements
    printf("Array elements:\n");
    for (i = 0; i < n; i++)
    {
        printf("arr[%d] = %d\n", i, array[i]);
    }
    printf("\n");

    // Free the allocated memory
    free(array);

    return 0;
}
