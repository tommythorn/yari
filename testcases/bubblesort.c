int sorted[99];

int main(void)
{
        int random = 1729, inc = 2345678901;
        int size = 0;
        int i;

        for (; size < 10;) {
                int swapped;
                sorted[size++] = 255 & random;
                random += inc;
                inc += inc + 1;

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

                if (0) {
                        for (i = 0; i < size; ++i)
                                printf("%d ", sorted[i]);
                        printf("\n");
                }
        }

        for (;;);

        return 0;
}
