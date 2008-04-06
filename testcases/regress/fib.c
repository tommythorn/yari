#include <stdio.h>

int fib(int n)
{
    if (n < 2)
        return n;
    else
        return fib(n - 1) + fib(n - 2);
}

int main()
{
    int i;

    printf("Fibonacci numbers:\n");

    for (i = 1; i < 8; ++i)
        printf("%d: %d\n", i, fib(i));
}
