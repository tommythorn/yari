#include <assert.h>
volatile unsigned a;
int main(int argc, char *argv[]) {
      unsigned b;
  a=0x27; b=0x6; assert((a + b) == 0x0000002d);
  return 0;
}
