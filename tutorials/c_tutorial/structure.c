#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
// #include <conio.h>


#define BUFSIZE 100
char buf[BUFSIZE]; /* buffer for ungetch */
int bufp = 0;      /* next free position in buf */
int getch(void);    /* get a (possibly pushed-back) character */
void ungetch(int);

struct point
{
    int x;
    int y;
};

#define NKEYS 10
#define MAXWORD 100

/* makepoint: make a point from x and y components */
struct point makepoint(int x, int y)
{
    struct point temp;
    temp.x = x;
    temp.y = y;
    return temp;
}



struct key
{
    char *word;
    int count;
};
// struct key keytab[NKEYS];

// struct key
// {
//     char *word;
//     int count;
// } keytab[NKEYS];


int binsearch(int x, int v[], int n);
void test_binsearch();
int getword(char *, int);
int binsearch_word(char *word, struct key tab[], int n);




int run_binsearch_word()
{
    struct key keytab[] = {
        "auto", 0,
        "break", 0,
        "case", 0,
        "char", 0,
        "const", 0,
        "continue", 0,
        "default", 0,
        /* ... */
        "unsigned", 0,
        "void", 0,
        "volatile", 0,
        "while", 0};

    int n;
    char word[MAXWORD];
    while (getword(word, MAXWORD) != EOF)
        if (isalpha(word[0]))
            if ((n = binsearch_word(word, keytab, NKEYS)) >= 0)
                keytab[n].count++;
    for (n = 0; n < NKEYS; n++)
        if (keytab[n].count > 0)
            printf("%4d %s\n", keytab[n].count, keytab[n].word);
    return 0;
}

void main(void)
{
    // test_binsearch();
    // initial a struct
    // struct point pt1 = {320, 200};
    // printf("struct point type pt1:(%d, %d)\n", pt1.x, pt1.y);
    // struct point pt2;
    // pt2.x == 100;
    // pt2.y = 200;
    // printf("struct point type pt2:(%d, %d)\n", pt2.x, pt2.y);

    // struct point pt3 = makepoint(1, 2);
    // printf("struct point type pt3:(%d, %d)\n", pt3.x, pt3.y);

    // struct point origin = makepoint(10, 20);
    // struct point *pp;
    // pp = &origin;
    // printf("origin is (%d,%d)\n", (*pp).x, (*pp).y);
    // printf("origin is (%d,%d)\n", pp->x, pp->y);

    run_binsearch_word();
}


int getch(void)    /* get a (possibly pushed-back) character */
{
    return (bufp > 0) ? buf[--bufp] : getchar();
}
void ungetch(int c) /* push character back on input */
{
    if (bufp >= BUFSIZE)
        printf("ungetch: too many characters\n");
    else
        buf[bufp++] = c;
}

/* binsearch:
find the index x in v[0] <= v[1] <= ... <= v[n-1]
if matched, return the index, otherwise return -1
*/
int binsearch(int x, int v[], int n)
{
    int low, high, middle, location;
    low = 0;
    high = n - 1;
    location = -1;
    while (low <= high)
    {
        middle = (low + high) / 2;
        if (x < v[middle])
        {
            high = middle - 1;
        }
        else if (x > v[middle])
        {
            low = middle + 1;
        }
        else
        {
            location = middle;
            break;
        }
    }
    return location;
}

/* binsearch: find word in tab[0]...tab[n-1] */
int binsearch_word(char *word, struct key tab[], int n)
{
    int cond;
    int low, high, mid;
    low = 0;
    high = n - 1;
    while (low <= high)
    {
        mid = (low + high) / 2;
        if ((cond = strcmp(word, tab[mid].word)) < 0)
            high = mid - 1;
        else if (cond > 0)
            low = mid + 1;
        else
            return mid;
    }
    return -1;
}

// Unit test function
void test_binsearch()
{
    int arr1[] = {1, 3, 5, 7, 9};
    int n1 = sizeof(arr1) / sizeof(arr1[0]);

    // Test cases with expected outputs
    assert(binsearch(5, arr1, n1) == 2);   // 5 is at index 2
    assert(binsearch(1, arr1, n1) == 0);   // 1 is at index 0
    assert(binsearch(9, arr1, n1) == 4);   // 9 is at index 4
    assert(binsearch(6, arr1, n1) == -1);  // 6 is not in arr1
    assert(binsearch(-1, arr1, n1) == -1); // -1 is not in arr1
    assert(binsearch(10, arr1, n1) == -1); // 10 is not in arr1

    int arr2[] = {};
    int n2 = sizeof(arr2) / sizeof(arr2[0]);

    // Test with an empty array
    assert(binsearch(1, arr2, n2) == -1);

    int arr3[] = {2};
    int n3 = sizeof(arr3) / sizeof(arr3[0]);

    // Test with a single-element array
    assert(binsearch(2, arr3, n3) == 0);  // 2 is at index 0
    assert(binsearch(3, arr3, n3) == -1); // 3 is not in arr3

    int arr4[] = {2, 2, 2, 2};
    int n4 = sizeof(arr4) / sizeof(arr4[0]);

    // Test with all elements being the same
    assert(binsearch(2, arr4, n4) >= 0);  // Should return a valid index (any index with 2)
    assert(binsearch(3, arr4, n4) == -1); // 3 is not in arr4

    printf("All tests passed!\n");
}

/* getword: get next word or character from input */
int getword(char *word, int lim)
{
    int c, getch(void);
    void ungetch(int);
    char *w = word;
    while (isspace(c = getch()))
        ;
    if (c != EOF)
        *w++ = c;
    if (!isalpha(c))
    {
        *w = '\0';
        return c;
    }
    for (; --lim > 0; w++)
        if (!isalnum(*w = getch()))
        {
            ungetch(*w);
            break;
        }
    *w = '\0';
    return word[0];
}
