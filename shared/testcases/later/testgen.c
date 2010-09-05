#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NTESTS 200

#define TRY(exp) \
        printf("  a=0x%08x; taint(&b); b=0x%08x; assert((%s) == 0x%08x);\n", a, b, #exp, exp)

int main(int argc, char *argv[])
{
  int i;

  printf("#include <assert.h>\n"
         "void taint(unsigned *x) { asm(\"\" : \"=r\" (x)); } \n"
         "int main(int argc, char *argv[]) {\n"
         "      unsigned a;\n"
         "      unsigned b;\n");

  for (i = 0; i < NTESTS; ++i) {
    unsigned a = random();
    unsigned b = random();

    if (random() % 2) {
      (int) a %= 64;
      (int) b %= 64;
    }

    TRY(a + b);
    TRY(a - b);
    TRY(a * b);
    TRY((int) a * (int) b);
    if (b) {
      TRY(a / b);
      TRY(a % b);
      TRY((int) a / (int) b);
      TRY((int) a % (int) b);
    }
    TRY(a & b);
    TRY(a | b);
    TRY(a ^ b);
    TRY(a ^ ~b);
    TRY(a | ~b);
    TRY(a << b);
    TRY(a >> b);
    TRY((int) a >> (int) b);
    TRY(a > b);
    TRY(a >= b);
    TRY(a < b);
    TRY(a <= b);
    TRY(a == b);
    TRY(a != b);
    TRY(a < 0);
    TRY(a <= 0);
    TRY((int) a >  (int) b);
    TRY((int) a >= (int) b);
    TRY((int) a <  (int) b);
    TRY((int) a <= (int) b);
    TRY((int) a == (int) b);
    TRY((int) a != (int) b);
    TRY((int) a <  (int) 0);
    TRY((int) a <= (int) 0);
  }

  printf("  return 0;\n"
         "}\n");

  return 0;
}
