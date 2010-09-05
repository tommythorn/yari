#include <stdio.h>

int sorted[99];

int main(void)
{
    int swapped;
    int size;
    int i;

    for (size = 0; size < 50; ++size)
        sorted[size] = random() & 0xFFFF;

    do {
        swapped = 0;
        for (i = 0; i < size-1; ++i)
            if (sorted[i] > sorted[i+1]) {
                int tmp = sorted[i];
                sorted[i] = sorted[i+1];
                sorted[i+1] = tmp;
                swapped = 1;
            }
    } while (swapped);

    for (i = 0; i < size; ++i)
        printf("%d ", sorted[i]);
    printf("\n");
}
