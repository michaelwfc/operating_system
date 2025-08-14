#include <stdio.h>

/* copy input to output; 1st version
Common Key Combinations for EOF:
On Unix-like systems (Linux, macOS): Press Ctrl+D
On Windows: Press Ctrl+Z followed by Enter

If you are running this on Windows, pressing Enter will count as two characters due to the CRLF (Carriage Return Line Feed) sequence.
If you want to handle this specifically, you could add code to ignore the carriage return character:
*/

#define MAXLINE 1000 /* maximum input line length */

int getline1(char line[], int maxline);
void copy(char to[], char from[]);

int get_longest_line(void);

int find_matched_lines(void);
int strindex(char source[], char searchfor[]);
char pattern[] = "ould"; /* pattern to search for */

void count_chars(void)
{
    printf("start to count characters:\n");
    int c; // We can't use char since c must be big enough to hold EOF in addition to any possible char. Therefore we use int
    long nc;
    nc = 0;
    // while ((c = getchar()) != EOF)
    while ((c = getchar()) != EOF)
    {
        // putchar(c);
        if (c != '\n') // Ignore carriage return characters
        {
            ++nc;
            printf("input char is: %c\nthe interger of %c= %d\ncount = %ld\n", c, c, c, nc);
        }
    }

    printf("EOF detected.\n");
}

void count_lines(void)
{

    /* count lines in input */
    printf("start to count lines:\n");
    int c, nl;
    nl = 0;
    while ((c = getchar()) != EOF)
        if (c == '\n')
        {
            ++nl;
            printf("line num=%d\n", nl);
        }
    printf("%d\n", nl);
    printf("EOF detected.\n");
}

#define IN 1  /* inside a word */
#define OUT 0 /* outside a word */

/* count lines, words, and characters in input */
int count_words(void)
{
    printf("start to count words:\n");
    int c, nl, nw, nc, state;
    state = OUT;
    nl = nw = nc = 0;
    while ((c = getchar()) != EOF)
    {
        ++nc;
        if (c == '\n')
            ++nl;
        if (c == ' ' || c == '\n' || c == '\t')
            state = OUT;
        else if (state == OUT)
        {
            state = IN;
            ++nw;
        }
    }
    printf("nl=%d, nw=%d,  nc=%d\n", nl, nw, nc);
    printf("EOF detected.\n");
}

/* Chapter 1.9 print the longest input line */
int get_longest_line(void)
{
    printf("get and print the longest input line:\n");
    int len;               /* current line length */
    int max;               /* maximum length seen so far */
    char line[MAXLINE];    /* current input line */
    char longest[MAXLINE]; /* longest line saved here */
    max = 0;
    while ((len = getline1(line, MAXLINE)) > 0)
        if (len > max)
        {
            max = len;
            copy(longest, line);
        }
    if (max > 0) /* there was a line */
    {
        printf("the longest input line length=%d : \n%s", max, longest);
    }
    else
    {
        printf("No input lines found.\n");
    }
    return 0;
}

/* getline: read a line into s, return length */
int getline1(char s[], int lim)
{
    int c, i;
    for (i = 0; i < lim - 1 && (c = getchar()) != EOF && c != '\n'; ++i)
        s[i] = c;
    if (c == '\n')
    {
        s[i] = c;
        ++i;
    }
    s[i] = '\0'; // the null character, whose value is zero
    return i;
}

/* copy: copy 'from' into 'to'; assume to is big enough */
void copy(char to[], char from[])
{
    int i;
    i = 0;
    while ((to[i] = from[i]) != '\0')
        ++i;
}

/* Chapter4.1: find all lines matching pattern */
int find_matched_lines(void)
{
    printf("find all lines matching pattern: %s\n", pattern);
    char line[MAXLINE];
    int found = 0;
    while (getline1(line, MAXLINE) > 0)
        if (strindex(line, pattern) >= 0)
        {
            printf("%s", line);
            found++;
        }
    if (found == 0)
    {
        printf("Not found!\n");
    }
    else
    {
        printf("%d lines found.\n", found);
    }
    return found;
}

/* strindex: return index of t in s, -1 if none */
int strindex(char s[], char t[])
{
    int i, j, k;
    for (i = 0; s[i] != '\0'; i++)
    {
        for (j = i, k = 0; t[k] != '\0' && s[j] == t[k]; j++, k++)
            ;
        if (k > 0 && t[k] == '\0')
            return i;
    }
    return -1;
}

/**
 * Examples from Chapter5.5  Character Pointers and Functions
 *
 */
void char_pointer_examples()
{
    char *pmessage;
    pmessage = "hello world"; // assigns to pmessage a pointer to the character array. This is not a string copy; only pointers are involved.

    printf("%s\n", pmessage);

    char amessage[] = "now is the time"; /* an array */
    char *pmessage2 = "now is the time";  /* a pointer */
}

/* strcpy: copy t to s; array subscript version */
void strcpy(char *s, char *t)
{
    int i;
    i = 0;
    while ((s[i] = t[i]) != '\0')
        i++;
}

/* strcpy: copy t to s; pointer version */
void strcpy1(char *s, char *t)
{
    int i;
    i = 0;
    while ((*s = *t) != '\0')
    {
        s++;
        t++;
    }
}

/* strcpy: copy t to s; pointer version 2 */
void strcpy2(char *s, char *t)
{
    while ((*s++ = *t++) != '\0')
        ;
}

/* strcpy: copy t to s; pointer version 3 */
void strcpy3(char *s, char *t)
{
    while (*s++ = *t++)
        ;
}

/* strcmp: return <0 if s<t, 0 if s==t, >0 if s>t */
int strcmp1(char *s, char *t)
{
    int i;
    //continue looping while characters at position i in both strings are equal.
    for (i = 0; s[i] == t[i]; i++)
        if (s[i] == '\0')
            return 0;
    return s[i] - t[i];
}

/*pointer version 2
* 

*p++ = val; /* push val onto stack 
val = *--p; /* pop top of stack into val
are the standard idiom for pushing and popping a stack; see Section 4.3.
*/
int strcmp2(char *s, char *t)
{
    for(;*s == *t; s++, t++)
        if(*s == '\0')
            return 0;
    return *s - *t; 
}



/**
 * Exercise 5-3. Write a pointer version of the function strcat that we showed in Chapter 2: strcat(s,t) copies the string t to the end of s.
 * 2.8 Increment and Decrement Operators
 * strcat: concatenate t to end of s; s must be big enough
 */

 void strcat1(char s[], char t[])
{ 
    int i, j;
    i= j =0;
    while(s[i] != '\0') //find end of s
        i++;
    // As each member is copied from t to s, the postfix ++ is applied to both i and j to make sure that they are in position for the next pass through the loop.
    while((s[i++] = t[j++]) != '\0') //copy t
        ;}

void strcat2(char *s, char *t)
{
    while(*s != '\0')
        s++;
    while((*s++ = *t++) != '\0')
        ;
}

int main()
{
    // count_chars();
    // count_lines();
    // count_words();

    // get_longest_line();
    // find_matched_lines();

    // char_pointer_examples();

    char s1[100] = "hello";
    char s2[100] = "world";
    printf("str concat by pointer\n");
    strcat2(s1, s2);
    printf("%s\n", s1);
}