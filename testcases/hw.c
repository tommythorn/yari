static void ser_out(unsigned ch)
{
#define SER_OUTBUSY() (*(volatile unsigned *)0xFF000000 != 0)
#define SER_OUT(data) (*(volatile unsigned *)0xFF000000 = (data))
  while (SER_OUTBUSY())
    ;
  SER_OUT(ch);
}

main()
{
        ser_out('S');
        ser_out('U');
        ser_out('C');
        ser_out('C');
        ser_out('E');
        ser_out('S');
        ser_out('S');
        ser_out('\r');
        ser_out('\n');
}
