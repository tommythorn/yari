public class test_return_subtype_violated {

    public static void main(String[] args) {
        TestController ct = new TestController();

        TestLoader ld1 = new TestLoader(ClassLoader.getSystemClassLoader(), "ld1", ct);
        TestLoader ld2 = new TestLoader(ClassLoader.getSystemClassLoader(), "ld2", ct);

        ld1.addClassfile("Foo", "classes1/Foo.class");
        ld1.addClassfile("DerivedFoo", "classes2/DerivedFoo.class");
        ld1.addParentDelegation("java.lang.Object");
        ld1.addParentDelegation("java.lang.String");

        ld2.addClassfile("BarPassFoo", "classes2/BarPassFoo.class");
        ld2.addClassfile("Foo", "classes2/Foo.class");
        ld2.addDelegation("DerivedFoo", ld1);
        ld2.addParentDelegation("java.lang.Object");
        ld2.addParentDelegation("java.lang.String");

        // loading BarPassFoo
        ct.expect("requested", ld2, "BarPassFoo");
        ct.expect("defined", ld2, "<BarPassFoo>");
        ct.expect("loaded", ld2, "<BarPassFoo>");

        Class cls = ct.loadClass(ld2, "BarPassFoo");

        // linking BarPassFoo
        ct.expectLoadFromSystem(ld2, "java.lang.Object");

        // executing createDerivedFoo
        ct.expectDelegationAndDefinition(ld2, ld1, "DerivedFoo");
        // ...linking (ld2, DerivedFoo)
        ct.expect("requested", ld1, "Foo");
        ct.expect("defined", ld1, "<Foo>");
        ct.expectLoadFromSystem(ld1, "java.lang.Object");

        ct.checkStringGetter(cls, "getDerivedFoo", "no exception");
        ct.expectEnd();

        // subtype check (DerivedFoo subtypeof Foo)
        ct.expect("requested", ld2, "Foo");
        ct.expect("defined", ld2, "<Foo>");
        // ... linking (ld2, Foo), j.l.O is already loaded

        // the subtype constraint ((ld2, DerivedFoo) subtypeof (ld2, Foo)) is violated
        ct.expect("exception", "java.lang.LinkageError", "<BarPassFoo>");

        ct.checkStringGetterMustFail(cls, "getDerivedFooAsFoo");

        ct.exit();
    }

}

// vim: et sw=4
