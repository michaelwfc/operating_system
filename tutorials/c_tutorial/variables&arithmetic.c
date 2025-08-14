
#include <stdio.h>

#define LOWER 0 /* lower limit of table */
#define UPPER 300 /* upper limit */
#define STEP 20 /* step size */


/* print Fahrenheit-Celsius table
for fahr = 0, 20, ..., 300 */
int main(void)
{
    // int fahr, celsius;
    // int lower, upper, step;
    float fahr, celsius;
    float lower, upper, step;

    lower = 0;   /* lower limit of temperature scale */
    upper = 300; /* upper limit */
    step = 20;   /* step size */
    fahr = lower;
    printf("Fahrenheit-Celsius table\n");
    while (fahr <= upper)
    {
        celsius = 5 * (fahr - 32) / 9;
        // printf("%3d\t%6d\n", fahr, celsius); //%d specifies an integer argument,
        printf("%3.0f\t%6.1f\n", fahr, celsius);

        fahr = fahr + step;
    }
}