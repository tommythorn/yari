public class test_retval_loading_constraint_violated {

    public static void main(String[] args) {
        TestController ct = new TestController();

        TestLoader ld1 = new TestLoader(ClassLoader.getSystemClassLoader(), "ld1", ct);
        TestLoader ld2 = new TestLoader(ClassLoader.getSystemClassLoader(), "ld2", ct);

        ld1.addClassfile("BarUseFoo", "classes1/BarUseFoo.class");
        ld1.addClassfile("Foo", "classes1/Foo.class");
        ld1.addDelegation("BarPassFoo", ld2);
        ld1.addParentDelegation("java.lang.Object");
        ld1.addParentDelegation("java.lang.String");

        ld2.addClassfile("BarPassFoo", "classes2/BarPassFoo.class");
        ld2.addClassfile("Foo", "classes2/Foo.class");
        ld2.addParentDelegation("java.lang.Object");
        ld2.addParentDelegation("java.lang.String");


        // loading BarUseFoo
        ct.expect("requested", ld1, "BarUseFoo");
        ct.expect("defined", ld1, "<BarUseFoo>");
        ct.expect("loaded", ld1, "<BarUseFoo>");

        Class cls = ct.loadClass(ld1, "BarUseFoo");

        // linking BarUseFoo
        ct.expectLoadFromSystem(ld1, "java.lang.Object");

        // executing BarUseFoo.useReturnedFoo: new BarPassFoo
        ct.expectDelegationAndDefinition(ld1, ld2, "BarPassFoo");
        // ...linking BarPassFoo
        ct.expectLoadFromSystem(ld2, "java.lang.Object");

        // resolving BarPassFoo.createFoo
        ct.expect("requested", ld2, "Foo");
        ct.expect("defined", ld2, "<Foo>");
        ct.expect("requested", ld1, "Foo");
        // ...the loading constraing (ld1,ld2,Foo) is violated
        ct.expect("exception", "java.lang.LinkageError", "<BarUseFoo>");

        ct.checkStringGetterMustFail(cls, "useReturnedFoo");

        ct.exit();
    }

}

// vim: et sw=4
