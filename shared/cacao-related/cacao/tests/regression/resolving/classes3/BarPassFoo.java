public class BarPassFoo {
    public static String id() {
        return "classes3/BarPassFoo";
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
}

// vim: et sw=4
