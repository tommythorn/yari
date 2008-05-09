public class BarUseFoo {
    public static String id() {
        return "classes1/BarUseFoo";
    }

    public static String idOfFoo() {
        return Foo.id();
    }

    public String useFoo(Foo foo) {
        return foo.virtualId();
    }

    public static String useReturnedFoo() {
        BarPassFoo bpf = new BarPassFoo();
        Foo foo = bpf.createFoo();
        return foo.virtualId();
    }
}

// vim: et sw=4

