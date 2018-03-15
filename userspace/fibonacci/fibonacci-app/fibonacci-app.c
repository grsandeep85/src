/* Simple application to calculate the terms of the Fibonacci series
 * (c) 2017 Rudolf J Streif, rudolf.streif@ibeeto.com
 */

#include<stdio.h>
 
int main()
{
   unsigned int n, first = 0, second = 1, next;
 
   printf("Enter the number of terms: ");
   scanf("%d",&n);
 
   printf("First %d terms of Fibonacci series are:\n",n);
 
   for (unsigned int c = 0 ; c < n ; c++)
   {
      if (c <= 1)
         next = c;
      else
      {
         next = first + second;
         first = second;
         second = next;
      }
      printf("%d ", next);
   }
   printf("\n");
 
   return 0;
}
