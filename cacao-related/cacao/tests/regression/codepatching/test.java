public class test extends Thread {
    static boolean doit = true;

    public static void main(String[] argv) {
        int threadcount = 1;

        if (argv.length > 0) {
            for (int i = 0; i < argv.length; i++) {
                if (argv[i].equals("--help")) {
                    usage();

                } else if (argv[i].equals("skip")) {
                    doit = false;

                } else {
                    threadcount = Integer.valueOf(argv[i]).intValue();
                }
            }
        }

        System.out.println("Running with " + threadcount + " threads.");

        for (int i = 0; i < threadcount; i++) {
            new test().start();
        }
    }

    static void usage() {
        System.out.println("test [number of threads] [skip]");
        System.exit(1);
    }

    public test() {
    }

    public void start() {
        run();
    }

    public void run() {
        invokestatic();

        getstatic();
        putstatic();
        putstaticconst();

        getfield();
        putfield();
        putfieldconst();

        newarray();
        multianewarray();

        invokespecial();

        checkcast();
        _instanceof();

        aastoreconst();
    }


    final private static void invokestatic() {
        try {
            p("invokestatic: ");
            if (doit)
                invokestatic.sub();
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }


    private void getstatic() {
        try {
            p("getstatic (I): ");
            if (doit)
                check(getstaticI.i, 123);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("getstatic (J): ");
            if (doit)
                check(getstaticJ.l, 1234567890123L);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("getstatic (F): ");
            if (doit)
                check(getstaticF.f, 123.456F);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("getstatic (D): ");
            if (doit)
                check(getstaticD.d, 789.012);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("getstatic (L): ");
            if (doit)
                check(getstaticL.o, null);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void putstatic() {
        try {
            p("putstatic (I): ");
            if (doit) {
                int i = 123;
                putstaticI.i = i;
                check(putstaticI.i, i);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstatic (J): ");
            if (doit) {
                long l = 1234567890123L;
                putstaticJ.l = l;
                check(putstaticJ.l, l);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstatic (F): ");
            if (doit) {
                float f = 123.456F;
                putstaticF.f = f;
                check(putstaticF.f, f);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstatic (D): ");
            if (doit) {
                double d = 789.012;
                putstaticD.d = d;
                check(putstaticD.d, d);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }


        try {
            p("putstatic (L): ");
            if (doit) {
                Object o = null;
                putstaticL.o = o;
                check(putstaticL.o, o);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void putstaticconst() {
        try {
            p("putstaticconst (I): ");
            if (doit) {
                putstaticconstI.i = 123;
                check(putstaticconstI.i, 123);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst (J): ");
            if (doit) {
                putstaticconstJ.l = 1234567890123L;
                check(putstaticconstJ.l, 1234567890123L);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst (F): ");
            if (doit) {
                putstaticconstF.f = 123.456F;
                check(putstaticconstF.f, 123.456F);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst (D): ");
            if (doit) {
                putstaticconstD.d = 789.012;
                check(putstaticconstD.d, 789.012);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst zero (I): ");
            if (doit) {
                putstaticconstI.i = 0;
                check(putstaticconstI.i, 0);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst zero (J): ");
            if (doit) {
                putstaticconstJ.l = 0L;
                check(putstaticconstJ.l, 0L);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst zero (F): ");
            if (doit) {
                putstaticconstF.f = 0.0F;
                check(putstaticconstF.f, 0.0F);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst zero (D): ");
            if (doit) {
                putstaticconstD.d = 0.0;
                check(putstaticconstD.d, 0.0);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst zero (L): ");
            if (doit) {
                putstaticconstL.o = null;
                check(putstaticconstL.o, null);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putstaticconst unresolved class: ");
            if (doit) {
                putstaticconstC.c = putstaticconstC.class;
                check(putstaticconstC.c, Class.forName("putstaticconstC"));
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        } catch (ClassNotFoundException t) {
            failed(t);
        }
    }

    private void getfield() {
        try {
            p("getfield (I): ");
            if (doit)
                check(new getfieldI().i, 123);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("getfield (J): ");
            if (doit)
                check(new getfieldJ().l, 1234567890123L);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("getfield (F): ");
            if (doit)
                check(new getfieldF().f, 123.456F);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("getfield (D): ");
            if (doit)
                check(new getfieldD().d, 789.012);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("getfield (L): ");
            if (doit)
                check(new getfieldL().o, null);
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void putfield() {
        try {
            p("putfield (I): ");
            if (doit) {
                putfieldI pfi = new putfieldI();
                int i = 123;
                pfi.i = i;
                check(pfi.i, i);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfield (J): ");
            if (doit) {
                putfieldJ pfj = new putfieldJ();
                long l = 1234567890123L;
                pfj.l = l;
                check(pfj.l, l);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfield (F): ");
            if (doit) {
                putfieldF pff = new putfieldF();
                float f = 123.456F;
                pff.f = f;
                check(pff.f, f);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfield (D): ");
            if (doit) {
                putfieldD pfd = new putfieldD();
                double d = 789.012;
                pfd.d = d;
                check(pfd.d, d);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfield (L): ");
            if (doit) {
                putfieldL pfl = new putfieldL();
                Object o = null;
                pfl.o = o;
                check(pfl.o, o);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void putfieldconst() {
        try {
            p("putfieldconst (I): ");
            if (doit) {
                putfieldconstI pfci = new putfieldconstI();
                pfci.i = 123;
                check(pfci.i, 123);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
 
        try {
            p("putfieldconst (J): ");
            if (doit) {
                putfieldconstJ pfcj = new putfieldconstJ();
                pfcj.l = 1234567890123L;
                check(pfcj.l, 1234567890123L);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfieldconst (F): ");
            if (doit) {
                putfieldconstF pfcf = new putfieldconstF();
                pfcf.f = 123.456F;
                check(pfcf.f, 123.456F);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
 
        try {
            p("putfieldconst (D): ");
            if (doit) {
                putfieldconstD pfcd = new putfieldconstD();
                pfcd.d = 789.012;
                check(pfcd.d, 789.012);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfieldconst zero (I): ");
            if (doit) {
                putfieldconstI pfci = new putfieldconstI();
                pfci.i = 0;
                check(pfci.i, 0);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
 
        try {
            p("putfieldconst zero (J): ");
            if (doit) {
                putfieldconstJ pfcj = new putfieldconstJ();
                pfcj.l = 0L;
                check(pfcj.l, 0L);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfieldconst zero (F): ");
            if (doit) {
                putfieldconstF pfcf = new putfieldconstF();
                pfcf.f = 0.0F;
                check(pfcf.f, 0.0F);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
 
        try {
            p("putfieldconst zero (D): ");
            if (doit) {
                putfieldconstD pfcd = new putfieldconstD();
                pfcd.d = 0.0;
                check(pfcd.d, 0.0);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfieldconst zero (L): ");
            if (doit) {
                putfieldconstL pfcl = new putfieldconstL();
                pfcl.o = null;
                check(pfcl.o, null);
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("putfieldconst unresolved class: ");
            if (doit) {
                putfieldconstC pfcc = new putfieldconstC();
                pfcc.c = putfieldconstC.class;
                check(pfcc.c, Class.forName("putfieldconstC"));
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        } catch (ClassNotFoundException t) {
            failed(t);
        }
    }

    private void newarray() {
        try {
            p("newarray: ");
            if (doit) {
                newarray[] na = new newarray[1];
            }
            ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void multianewarray() {
        try {
            p("multianewarray: ");
            if (doit) {
                multianewarray[][] ma = new multianewarray[1][1];
            }
            ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void invokespecial() {
        try {
            p("invokespecial: ");
            if (doit)
                new invokespecial();
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void checkcast() {
        Object o = new Object();

        // class
        try {
            p("checkcast class: ");
            if (doit) {
                checkcastC cc = (checkcastC) o;
                failed();
            } else
                ok();
        } catch (ClassCastException e) {
            ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        // interface
        try {
            p("checkcast interface: ");
            if (doit) {
                checkcastI ci = (checkcastI) o;
                failed();
            } else
                ok();
        } catch (ClassCastException e) {
            ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }


        // array

        Object[] oa = new Object[1];

        try {
            p("checkcast class array: ");
            if (doit) {
                checkcastC[] cca = (checkcastC[]) oa;
                failed();
            } else
                ok();
        } catch (ClassCastException e) {
            ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void _instanceof() {
        Object o = new Object();

        try {
            p("instanceof class: ");
            if (doit)
                if (o instanceof instanceofC)
                    failed();
                else
                    ok();
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }

        try {
            p("instanceof interface: ");
            if (doit)
                if (o instanceof instanceofI)
                    failed();
                else
                    ok();
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }


        // array

        Object[] oa = new Object[1];

        try {
            p("instanceof class array: ");
            if (doit)
                if (oa instanceof instanceofC[])
                    failed();
                else
                    ok();
            else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        }
    }

    private void aastoreconst() {
        Class[] ca = new Class[1];

        try {
            p("aastoreconst of unresolved class != NULL: ");
            if (doit) {
                ca[0] = aastoreconstClass.class;

                if (ca[0] != null)
                    ok();
                else
                    failed();

                p("aastoreconst of unresolved correct value: ");
                check(ca[0],Class.forName("aastoreconstClass"));
            } else
                ok();
        } catch (NoClassDefFoundError t) {
            failed(t);
        } catch (ClassNotFoundException t) {
            failed(t);
        }
    }

    private static final void ok() {
        pln("OK");
    }

    private static final void failed() {
        pln("FAILED");
    }

    private static final void failed(Throwable t) {
        pln("FAILED: " + t);
    }

    private static final void check(int a, int b) {
        if (a == b)
            ok();
        else
            pln("FAILED: " + a + " != " + b + " (0x" + Integer.toHexString(a) + " != 0x" + Integer.toHexString(b) + ")");
    }

    private static final void check(long a, long b) {
        if (a == b)
            ok();
        else
            pln("FAILED: " + a + " != " + b + " (0x" + Long.toHexString(a) + " != 0x" + Long.toHexString(b) + ")");
    }

    private static final void check(float a, float b) {
        if (a == b)
            ok();
        else
            pln("FAILED: " + a + " != " + b);
    }

    private static final void check(double a, double b) {
        if (a == b)
            ok();
        else
            pln("FAILED: " + a + " != " + b);
    }

    private static final void check(Object a, Object b) {
        if (a == b)
            ok();
        else
            pln("FAILED: " + a + " != " + b);
    }

    private static final void p(String s) {
        System.out.print(s);
    }

    private static final void pln(String s) {
        System.out.println(s);
    }
}

// vim: et ts=4 sw=4

