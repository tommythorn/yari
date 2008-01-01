double one = 1;
double tree = 3;

double fib(int n)
{
  if (n < 2)
    return 1;
  else {
    double a = fib(n-1);
    double b = fib(n-2);
    printf("%f + %f = %f\n", a, b, a+b);
    return a + b;
  }
}

main()
{
  int i;
  double d;

  printf("sizeof(double) = %d\n", sizeof(double));

  for (i = 0; i < 22; ++i) {
    d = (double) i;
    printf("%2d ~ %08x%08x\n", i, ((unsigned*)&d)[0], ((unsigned*)&d)[1]);
  }

  fib(10);
}
