#include<string.h>
#include<ctype.h>
#include<stdlib.h>
#include<stdio.h>

typedef char * String;

String copy(String s);

int main(void)
{
    // String s= "hi";
    char s[10] = "hi";

    // String t = s;
    // t[0] = toupper(t[0]);
    // printf("%s\n",s);
    // printf("%s\n",t);

    String p = copy(s); 
    
    printf("%s\n",s);
    printf("%s\n", p);
    free(p);
}


String copy(String s)
// asgin a new memory to copy a string
{   
    if(s == NULL)
    {
        return 1;
    }
    char* p = malloc(strlen(s) +1 );
    if (p == NULL)
    {
        return 1;
    }
    // same as strcopy
    for (int i=0,n = strlen(s);i<n;i++)
    {
        p[i] = s[i];
    }
    p[0] = toupper(p[0]);
}

