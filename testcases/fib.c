#include "standalone-common.h"

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

    puts("Fibonacci numbers:\n");

    for (i = 1; i < 8; ++i) {
        put_unsigned(i);
        puts(": ");
        put_unsigned(fib(i));
        puts("\n");
    }
}
