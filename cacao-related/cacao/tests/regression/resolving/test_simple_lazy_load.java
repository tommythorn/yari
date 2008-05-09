public class test_simple_lazy_load {

    public static void main(String[] args) {
        TestController ct = new TestController();

        TestLoader ld1 = new TestLoader(ClassLoader.getSystemClassLoader(), "ld1", ct);

        ld1.addClassfile("BarUseFoo", "classes1/BarUseFoo.class");
        ct.expect("requested", ld1, "BarUseFoo");
        ct.expect("defined", ld1, "<BarUseFoo>");
        ct.expect("loaded", ld1, "<BarUseFoo>");
        Class cls = ct.loadClass(ld1, "BarUseFoo");
        ct.expectEnd();

        ld1.addParentDelegation("java.lang.Object");
        ct.expectLoadFromSystem(ld1, "java.lang.Object");
        ct.checkClassId(cls, "classes1/BarUseFoo");
        ct.expectEnd();

        ld1.addClassfile("Foo", "classes1/Foo.class");
        ct.setReportClassIDs(true);
        ct.expect("requested", ld1, "Foo");
        ct.expect("defined", ld1, "<Foo:classes1/Foo>");
        ct.checkStringGetter(cls, "idOfFoo", "classes1/Foo");
        ct.expectEnd();

        ct.exit();
    }

}

// vim: et sw=4
