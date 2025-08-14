#include <stdio.h>

int main(void)
{
    int x[1024];
    for (int i=0;i<1024;i++)
    {
        printf("x[%i]=%i\n",i,x[i]);
    }
}