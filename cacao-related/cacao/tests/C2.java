class C2 extends A {
int cx;
void m1( ) {ax = 100; cx=1;
//D d = new D();
}
public static void main(String[] s) {
  A a;
  B b;
  int i=1;
if (i==1)
  a = new A();
else
  a = new D();

  a.m1();
  a.m2();
 } 
}

