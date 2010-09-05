class D extends A {
int ax = 133;

void m1( ) {ax = ax + 10; 
  // System.out.println("In D.m1: "+ax);
  }

B m3( ) {
B b = new B();
b.bx++;
return b;
}
}
