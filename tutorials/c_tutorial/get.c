#include <stdio.h>

int get_int(void);
char* get_string(void);


int main(void)
{
    // int n = get_int();
    // printf("n=%i\n",n);

    char *s =  get_string();
    
    printf("s=%s\n",s);
}

int get_int(void)
{
   printf("Input integer n: ");
   int num;
    // passging by reference
   scanf("%i", &num);
   
   return num;

}


char * get_string(void)
{
    printf("Input string: ");
    
    // char *s; // s is pointer to a grabae value
    static char s[3]; 
    
    scanf("%s", s);
    return s;
}
