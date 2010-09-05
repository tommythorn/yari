import java.util.Vector;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class TestController {

    boolean report_class_ids_ = false;
    Vector expectations_ = new Vector();
    boolean failed_ = false;

    class Expectation {
        String tag_;
        String loader1_;
        String loader2_;
        String class_;

        public Expectation(String tag, String ld1, String ld2, String cls) {
            tag_ = tag;
            loader1_ = ld1;
            loader2_ = ld2;
            class_ = cls;
        }

        public Expectation(String tag, String ld1, String cls) {
            this(tag, ld1, null, cls);
        }

        public boolean matches(String tag, String ld1, String ld2, String cls) {
            return tag_.equals(tag)
                && loader1_.equals(ld1)
                && ((loader2_ == ld2) || ((loader2_ != null) && (ld2 != null) && loader2_.equals(ld2)))
                && class_.equals(cls);
        }
    }

    public void setReportClassIDs(boolean rep) {
        report_class_ids_ = rep;
    }

    void expect(Expectation exp) {
        expectations_.add(exp);
    }

    void expect(String tag, String loader, String classname) {
        expect(new Expectation(tag, loader, classname));
    }

    void expect(String tag, String loader1, String loader2, String classname) {
        expect(new Expectation(tag, loader1, loader2, classname));
    }

    public void expect(String tag, ClassLoader loader, String classname) {
        expect(tag, loaderName(loader), classname);
    }

    public void expect(String tag, ClassLoader loader1, ClassLoader loader2, String classname) {
        expect(tag, loaderName(loader1), loaderName(loader2), classname);
    }

    public void expectLoadFromSystem(ClassLoader loader, String classname) {
        expect("requested", loader, classname);
        expect("delegated", loader, ClassLoader.getSystemClassLoader(), classname);
        expect("loaded", loader, "<" + classname + ">");
    }

    public void expectDelegationAndDefinition(ClassLoader loader1, ClassLoader loader2, String classname) {
        expect("requested", loader1, classname);
        expect("delegated", loader1, loader2, classname);
        expect("requested", loader2, classname);
        expect("defined", loader2, "<" + classname + ">");
        expect("loaded", loader1, "<" + classname + ">");
    }

    public void expectDelegationAndFound(ClassLoader loader1, ClassLoader loader2, String classname) {
        expect("requested", loader1, classname);
        expect("delegated", loader1, loader2, classname);
        expect("requested", loader2, classname);
        expect("found", loader2, "<" + classname + ">");
        expect("loaded", loader1, "<" + classname + ">");
    }

    void fail(String message) {
        log("FAIL: " + message);
        failed_ = true;
    }

    void ok(String message) {
        log("ok: " + message);
    }

    void fail(String message, String tag, String ld1, String ld2, String cls) {
        fail(message + ": " + tag + " " + ld1 + " " + ld2 + " class=" + cls);
    }

    void ok(String tag, String ld1, String ld2, String cls) {
        ok(tag + " " + ld1 + " " + ld2 + " class=" + cls);
    }

    public void expectEnd() {
        if (expectations_.size() != 0)
            fail("missing reports");
        else
            ok("got all expected reports");
    }

    public void checkStringGetter(Class cls, String methodname, String expected) {
        String id = invokeStringGetter(cls, methodname);
        if (id == null)
            fail("could not get return value of " + methodname + "()");
        else if (id.equals(expected))
            ok("returned string matches: " + id);
        else
            fail("wrong string returned: " + id + ", expected: " + expected);
    }

    public void checkStringGetterMustFail(Class cls, String methodname) {
        String id = invokeStringGetter(cls, methodname);
        if (id == null)
            ok("method invocation failed as expected: " + methodname + "()");
        else 
            fail("method invocation did not fail as expected: " + methodname + "()");
    }

    public void checkClassId(Class cls, String expected) {
        String id = getClassId(cls);
        if (id == null)
            fail("could not get class id");
        else if (id.equals(expected))
            ok("class id matches: " + id);
        else
            fail("wrong class id: " + id + ", expected: " + expected);
    }

    public synchronized void match(String tag, String ld1, String ld2, String cls) {
        if (expectations_.size() == 0) {
            fail("unexpected", tag, ld1, ld2, cls);
        }
        else {
            Expectation exp = (Expectation) expectations_.firstElement();

            if (exp.matches(tag, ld1, ld2, cls)) {
                expectations_.remove(0);
                ok(tag, ld1, ld2, cls);
            }
            else {
                fail("unexpected", tag, ld1, ld2, cls);
            }
        }
    }

    void report(String tag, String loader, String classname) {
        match(tag, loader, null, classname);
    }

    void report(String tag, String loader1, String loader2, String classname) {
        match(tag, loader1, loader2, classname);
    }

    void report(String tag, ClassLoader loader, String classname) {
        report(tag, loaderName(loader), classname);
    }

    void report(String tag, ClassLoader loader, Class cls) {
        report(tag, loaderName(loader), className(cls));
    }

    void report(String tag, ClassLoader loader1, ClassLoader loader2, String classname) {
        report(tag, loaderName(loader1), loaderName(loader2), classname);
    }

    public void reportRequest(ClassLoader loader, String classname) {
        report("requested", loader, classname);
    }

    public void reportUnexpectedClassRequested(ClassLoader loader, String classname) {
        report("unexpected class requested", loader, classname);
    }

    public void reportDelegation(ClassLoader loader, ClassLoader delegate, String classname) {
        report("delegated", loaderName(loader), loaderName(delegate), classname);
    }

    public void reportDefinition(ClassLoader loader, Class cls) {
        report("defined", loaderName(loader), className(cls));
    }

    public void reportFoundLoaded(ClassLoader loader, Class cls) {
        report("found", loaderName(loader), className(cls));
    }

    public void reportLoaded(ClassLoader loader, Class cls) {
        report("loaded", loaderName(loader), className(cls));
    }

    public void reportClassNotFound(ClassLoader loader, String classname, ClassNotFoundException e) {
        report("class not found", loaderName(loader), classname);
    }

    public void reportException(Class cls, Throwable ex) {
        report("exception", ex.getClass().getName(), className(cls));
        log("exception was: " + ex);
        // ex.printStackTrace(System.out);
    }

    public Class loadClass(ClassLoader loader, String classname) {
        try {
            Class cls = loader.loadClass(classname);

            reportLoaded(loader, cls);

            return cls;
        }
        catch (ClassNotFoundException e) {
            reportClassNotFound(loader, classname, e);
        }

        return null;
    }

    public void log(String str) {
        System.out.println(str);
    }

    public String loaderName(ClassLoader loader) {
        if (loader == ClassLoader.getSystemClassLoader())
            return "<SystemClassLoader>";

        return (loader == null) ? "<null>" : loader.toString();
    }

    public String invokeStringGetter(Class cls, String methodname) {
        try {
            Method mid = cls.getMethod(methodname, null);

            String id = (String) mid.invoke(null, null);

            return id;
        }
        catch (NoSuchMethodException e) {
            return null;
        }
        catch (InvocationTargetException e) {
            reportException(cls, e.getCause());
            return null;
        }
        catch (Exception e) {
            reportException(cls, e);
            return null;
        }
    }

    public String getClassId(Class cls) {
        return invokeStringGetter(cls, "id");
    }

    public String className(Class cls) {
        if (report_class_ids_) {
            String id = getClassId(cls);
            if (id != null)
                return "<" + cls.getName() + ":" + id + ">";
        }

        return "<" + cls.getName() + ">";
    }

    public void exit() {
        expectEnd();
        System.exit(failed_ ? 1 : 0);
    }

}

// vim: et sw=4
