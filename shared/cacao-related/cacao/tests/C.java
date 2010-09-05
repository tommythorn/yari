class C extends A {
static int cx = 1;

void m1( ) {ax = 100; cx=1;
}

public static void main(String[] s) {
  A a;
  B b = new B();
  a=b;
  b.m1();
  b.m2();
  cx++;
//  System.out.println("Hello World"));
//  System.out.println("C: "+ ax +"; B: "+b.ax +"; A: "+a.ax);
 } 
}

