import java.io.Serializable;

public class TestArrayClasses extends TestBase {
  public static class Foo {
  }

  public static class FooChild extends Foo {
  }
  
  public static void test_store(Object[] array,Object obj,boolean oktostore,String msg) {
    try {
      array[0] = obj;
      ok(oktostore,msg);
    }
    catch (ArrayStoreException x) {
      System.out.println("    ArrayStoreException");
      ok(!oktostore,msg);
    }
  }

  public static void test_clone() {
    int[] ia1 = new int[100];
    Integer[] Ia1 = new Integer[ia1.length];
    int i;
    
    for (i=0; i<ia1.length; ++i) {
      ia1[i] = i*i;
      Ia1[i] = new Integer(ia1[i]);
    }

    int[] ia2 = (int[]) ia1.clone();

    is(ia2.length,ia1.length,"cloned int array length");

    boolean eq = true;
    for (i=0; i<ia1.length; ++i) {
      if (ia2[i] != ia1[i])
        eq = false;
      //      System.out.println(ia2[i]);
    }
    ok(eq,"cloned int array data");

    Integer[] Ia2 = (Integer[]) Ia1.clone();
    
    is(Ia2.length,Ia1.length,"cloned Integer array length");
    
    eq = true;
    for (i=0; i<ia1.length; ++i) {
      if (Ia2[i].intValue() != ia1[i])
        eq = false;
      //      System.out.println(ia2[i]);
    }
    ok(eq,"cloned Integer array data");
  }

  public static void test_arraycopy() {
    int len;
    int i;

    long[] la1 = new long[1024];
    long[] la2 = (long[]) la1.clone();
    int size = la1.length;
    Long[] La1 = new Long[size];
    Long[] La2 = (Long[]) La1.clone();
    Number[] Na2 = new Number[size];

    for (i=0; i<la1.length; ++i) {
      la1[i] = i*i;
      La1[i] = new Long(la1[i]);
    }

    i = size;
    while (i>1) {
      if ((i & 1) != 0)
        System.err.println("ERROR: arracopy test only works for powers of two");
      i >>= 1;
    }
    
    // reverse array
    len = size / 2;
    while (len>0) {
      for (int j=0;j<(size/2)/len;++j) {
        //        System.err.println(len);
        //        System.err.println(j);
        System.arraycopy(la1,(2*j+1)*len,la2,2*j*len,len);
        System.arraycopy(la1,2*j*len,la1,(2*j+1)*len,len);
        System.arraycopy(la2,2*j*len,la1,2*j*len,len);
      }
      len /= 2;
    }

    boolean eq = true;
    for (i=0; i<size; ++i) {
      if (la1[i] != (size-i-1)*(size-i-1))
        eq = false;
      //      System.out.println(la1[i]);
    }
    ok(eq,"arraycopy primitive");
    
    // reverse array
    len = size / 2;
    while (len>0) {
      for (int j=0;j<(size/2)/len;++j) {
        //        System.err.println(len);
        //        System.err.println(j);
        System.arraycopy(La1,(2*j+1)*len,La2,2*j*len,len);
        System.arraycopy(La1,2*j*len,La1,(2*j+1)*len,len);
        System.arraycopy(La2,2*j*len,La1,2*j*len,len);
      }
      len /= 2;
    }

    eq = true;
    for (i=0; i<size; ++i) {
      if (La1[i].intValue() != (size-i-1)*(size-i-1))
        eq = false;
    }
    ok(eq,"arraycopy ref");
    
    // reverse array
    len = size / 2;
    while (len>0) {
      for (int j=0;j<(size/2)/len;++j) {
        //        System.err.println(len);
        //        System.err.println(j);
        System.arraycopy(La1,(2*j+1)*len,Na2,2*j*len,len);
        System.arraycopy(La1,2*j*len,La1,(2*j+1)*len,len);
        System.arraycopy(Na2,2*j*len,La1,2*j*len,len);
      }
      len /= 2;
    }

    eq = true;
    for (i=0; i<size; ++i) {
      if (La1[i].intValue() != i*i)
        eq = false;
    }
    ok(eq,"arraycopy ref different classes");

    Integer[] Ia = new Integer[size];

    try {
      System.arraycopy(Ia,0,Na2,0,1);
      ok(true,"arraycopy Integer to Number");
    }
    catch (ArrayStoreException x) {
      System.out.println("    ArrayStoreException");
      ok(false,"arraycopy Integer to Number");
    }

    try {
      System.arraycopy(Na2,1,Ia,1,1);
      ok(false,"!arraycopy Number to Integer");
    }
    catch (ArrayStoreException x) {
      System.out.println("    ArrayStoreException");
      ok(true,"!arraycopy Number to Integer");
    }
  }
  
  public static void main(String[] args) {
    int[] ia = new int[5];
    Object[] oa = new Object[2];
    Object[][] oaa = new Object[1][1];
    String[] sa = new String[3];
    String[][] saa = new String[1][1];
    String[][] saa2 = new String[2][];
    String[][][] saaa = new String[1][2][3];
    Object o = new Object();
    java.io.Serializable[] sera = new java.io.Serializable[1];
    Cloneable[] cloa = new Cloneable[1];
    StringBuffer[][] sbaa = new StringBuffer[1][1];
    Foo[] fooa = new Foo[1];
    FooChild[] fooca = new FooChild[1];

    Class[] ifs = String[].class.getInterfaces();
    is(ifs.length,2,"String[] implements 2 interfaces");
    ok(ifs[0] == java.lang.Cloneable.class || ifs[1] == java.lang.Cloneable.class,"String[] implements Cloneable");
    ok(ifs[0] == java.io.Serializable.class || ifs[1] == java.io.Serializable.class,"String[] implements Serializable");

    is(String[].class.getModifiers(),1041,"String[] is public final abstract");

    is(oa.getClass().getName(),"[Ljava.lang.Object;","classname ref");
    is(ia.getClass().getName(),"[I","classname primitive");
    is(ia.length,5,"arraylength primitive");
    is(oa.length,2,"arraylength ref");

    is(saa2.length,2,"arraylength of saa2");
    saa2[1] = new String[4];
    is(saa2[1].length,4,"arraylength of saa2[1]");

    is(saaa.length,1,"arraylength of saaa");
    is(saaa[0].length,2,"arraylength of saaa[0]");
    is(saaa[0][1].length,3,"arraylength of saaa[0][1]");

    ok(oa.getClass().isArray(),"Object[].isArray");
    ok(ia.getClass().isArray(),"int[].isArray");
    ok(!o.getClass().isArray(),"!Object.isArray");
    ok(!o.getClass().isPrimitive(),"!Object.isPrimitive");

    is(oa.getClass().getComponentType().getName(),"java.lang.Object","component ref");
    ok(!oa.getClass().getComponentType().isPrimitive(),"component ref !isPrimitive");
    is(ia.getClass().getComponentType().getName(),"int","component primitive");
    ok(ia.getClass().getComponentType().isPrimitive(),"component primitive isPrimitive");

    ok(saa.getClass().getComponentType().equals(sa.getClass()),"component of String[][] equals String[]");
    ok(!saa.getClass().getComponentType().equals(oa.getClass()),"component of String[][] !equals Object[]");

    ok(saa[0].getClass().equals(saa.getClass().getComponentType()),"saa[0].getClass equals component of String[][]");

    test_store(sa,new Object(),false,"!store Object in String[]");
    test_store(sa,new String("test"),true,"store String in String[]");
    test_store(oa,new Object(),true,"store Object in Object[]");
    test_store(oa,new String("test"),true,"store String in Object[]");

    test_store(oaa,sa,true,"store String[] in Object[][]");
    test_store(saa,oa,false,"!store Object[] in String[][]");

    test_store(sera,sa,true,"store String[] in java.io.Serializable[]");
    test_store(cloa,sa,true,"store String[] in Cloneable[]");
    
    test_store(sbaa,sa,false,"!store String[] in StringBuffer[][]");

    test_store(fooa,new Foo(),true,"store Foo in Foo[]");
    test_store(fooa,new FooChild(),true,"store FooChild in Foo[]");
    test_store(fooca,new Foo(),false,"!store Foo in FooChild[]");
    test_store(fooca,new FooChild(),true,"store FooChild in FooChild[]");
    
    try {
      Object[] oa2 = (Object[]) sa;
      ok(true,"cast String[] to Object[]");
    }
    catch (ClassCastException x) {
      System.out.println("    ClassCastException");
      ok(false,"cast String[] to Object[]");
    }

    try {
      String[] sa2 = (String[]) oa;
      ok(false,"!cast Object[] to String[]");
    }
    catch (ClassCastException x) {
      System.out.println("    ClassCastException");
      ok(true,"!cast Object[] to String[]");
    }

    ok(sa instanceof String[],"String[] instanceof String[]");
    ok(sa instanceof Object[],"String[] instanceof Object[]");
    ok(!(oa instanceof String[]),"Object[] !instanceof String[]");
    ok(oa instanceof Object[],"Object[] instanceof Object[]");

    ok(oaa instanceof Object[],"Object[][] instanceof Object[]");
    ok(saa instanceof Object[],"String[][] instanceof Object[]");

    ok(sa instanceof java.io.Serializable,"String[] instanceof java.io.Serializable");
    ok(sa instanceof java.lang.Cloneable,"String[] instanceof java.lang.Cloneable");
    ok(sa instanceof java.lang.Object,"String[] instanceof java.lang.Object");
    ok(saa[0] instanceof java.io.Serializable,"saa[0] instanceof java.io.Serializable");
    ok(saa[0] instanceof java.lang.Cloneable,"saa[0] instanceof java.lang.Cloneable");
    ok(saa[0] instanceof java.lang.Object,"saa[0] instanceof java.lang.Object");

    test_clone();
    test_arraycopy();
  }
}
