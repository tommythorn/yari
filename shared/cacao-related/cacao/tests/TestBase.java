public class TestBase {
  public static boolean ok(boolean test,String msg) {
    if (test) {
      System.out.println("ok "+msg);
      return true;
    }
    else {
      System.out.println("NOT ok "+msg);
      return false;
    }
  }

  public static boolean is(String a,String b,String msg) {
    if (ok(a.equals(b),msg)) return true;
    System.out.println("    a='"+a+"' b='"+b+"'");
    return false;
  }

  public static boolean is(int a,int b,String msg) {
    return ok(a == b,msg);
  }

  public static boolean equals(Object a,Object b,String msg) {
    return ok(a.equals(b),msg);
  }
}
