#include <stdio.h>

int foo;

int main(int argc, char **argv)
{
    int i;
    int k = random();
    int j = random();

    for (i = 0; i < 100; ++i, k += j, j += i)
        foo += ~(k | j);
}
