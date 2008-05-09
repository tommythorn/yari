public class BarPassFoo {
    public static String id() {
        return "classes2/BarPassFoo";
    }

    public static String passit() {
        Foo foo = new Foo();
        BarUseFoo bar = new BarUseFoo();

        return bar.useFoo(foo);
    }

    public static String passDerivedFoo() {
        DerivedFoo dfoo = new DerivedFoo();
        BarUseFoo bar = new BarUseFoo();

        return bar.useFoo(dfoo);
    }

    public static String passDerivedFooInstance() {
        Foo foo = new DerivedFoo();

        return foo.virtualId();
    }

    public Foo createFoo() {
        return new Foo();
    }

    public DerivedFoo createDerivedFoo() {
        return new DerivedFoo();
    }

    public Foo createDerivedFooReturnFoo() {
        return new DerivedFoo();
    }

    public static String getDerivedFoo() {
        BarPassFoo bar = new BarPassFoo();

        bar.createDerivedFoo();
        return "no exception";
    }

    public static String getDerivedFooAsFoo() {
        BarPassFoo bar = new BarPassFoo();

        bar.createDerivedFooReturnFoo();
        return "no exception";
    }
}

// vim: et sw=4
