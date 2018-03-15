/* Application to compute the terms of the Fibonacci Series
 * (c) 2017 Rudolf J Streif, rudolf.streif@ibeeto.com
 */

#include <stdio.h>
#include <stdlib.h>
#include "fibonacci.h"
 
int main()
{
    unsigned int terms;
    unsigned int* series;

    printf("Enter the number of terms: ");
    scanf("%d",&terms);

    if (!(series = calloc(terms, sizeof(unsigned int))))
        printf("Cannot allocate memory for series.\n");

    printf("First %d terms of Fibonacci series are:\n", terms);
    fibonacci(terms, series);
    for (unsigned int c = 0 ; c < terms ; c++)
        printf("%d ", series[c]);
    printf("\n");
    
    free(series);
    
    return 0;
}
