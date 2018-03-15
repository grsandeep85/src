/* Functions to compute the terms of the Fibonacci Series
 * (c) 2017 Rudolf J Streif, rudolf.streif@ibeeto.com
 */


void fibonacci(unsigned int terms, unsigned int* series)
{
    unsigned int first =0, second = 1, next;
    for (unsigned int c = 0 ; c < terms ; c++)
    {
        if (c <= 1)
            next = c;
        else
        {
            next = first + second;
            first = second;
            second = next;
        }
        series[c] = next;
    }
} 
