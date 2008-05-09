public class BarUseFoo {
    public static String id() {
        return "classes3/BarUseFoo";
    }

    public static String idOfFoo() {
        return Foo.id();
    }

    public String useFoo(Foo foo) {
        return "not implemented";
    }
}

// vim: et sw=4

