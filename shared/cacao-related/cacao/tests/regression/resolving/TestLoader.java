import java.util.Hashtable;
import java.io.*;

public class TestLoader extends ClassLoader {

    Hashtable registry_;
    String name_;
    TestController controller_;

    class Entry {
    }

    class ClassfileEntry extends Entry {
        public String filename_;
        public ClassfileEntry(String filename) { filename_ = filename; }
    }

    class DelegationEntry extends Entry {
        public ClassLoader loader_;
        public DelegationEntry(ClassLoader loader) { loader_ = loader; }
    }

    class SuperDelegationEntry extends Entry {
    }

    public TestLoader(ClassLoader parent, String name, TestController controller) {
        super(parent);
        name_ = name;
        controller_ = controller;
        registry_ = new Hashtable();
    }

    public void addClassfile(String classname, String filename) {
        registry_.put(classname, new ClassfileEntry(filename));
    }

    public void addDelegation(String classname, ClassLoader loader) {
        registry_.put(classname, new DelegationEntry(loader));
    }

    public void addParentDelegation(String classname) {
        registry_.put(classname, new DelegationEntry(getParent()));
    }

    public void addSuperDelegation(String classname) {
        registry_.put(classname, new SuperDelegationEntry());
    }

    public String toString() {
        return "TestLoader<" + name_ + ">";
    }

    public Class loadClass(String classname) throws ClassNotFoundException {
        controller_.reportRequest(this, classname);

        Entry entry = (Entry) registry_.get(classname);

        if (entry == null) {
            controller_.reportUnexpectedClassRequested(this, classname);
            throw new ClassNotFoundException(this + " does not know how to load class " + classname);
        }

        if (entry instanceof ClassfileEntry) {
            Class cls = findLoadedClass(classname);

            if (cls != null) {
                controller_.reportFoundLoaded(this, cls);
                return cls;
            }

            String filename = ((ClassfileEntry)entry).filename_;

            try {
                byte[] bytes = slurpFile(filename);

                cls = defineClass(classname, bytes, 0, bytes.length);

                controller_.reportDefinition(this, cls);

                return cls;
            }
            catch (Exception e) {
                throw new ClassNotFoundException(e.toString());
            }
        }
        else if (entry instanceof DelegationEntry) {
            ClassLoader delegate = ((DelegationEntry)entry).loader_;

            controller_.reportDelegation(this, delegate, classname);

            Class cls = delegate.loadClass(classname);

            controller_.reportLoaded(this, cls);

            return cls;
        }

        throw new ClassNotFoundException("unknown TestLoader entry: " + entry);
    }

    byte[] slurpFile(String filename) throws IOException {
        File file = new File(filename);
        InputStream is = new FileInputStream(file);
        long len = file.length();
        if (len > Integer.MAX_VALUE)
            throw new IOException("file " + file.getName() + " is too large");
        byte[] bytes = new byte[(int) len];

        int ofs = 0;
        int read = 0;
        while ((ofs < len) && (read = is.read(bytes, ofs, bytes.length - ofs)) >= 0)
            ofs += read;

        if (ofs < len)
            throw new IOException("error reading file " + file.getName());

        is.close();
        return bytes;
    }
}

// vim: et sw=4

