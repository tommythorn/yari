/* src/tests/native/checkjni.java - for testing JNI related stuff

   Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
   C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
   E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
   TU Wien

   This file is part of CACAO.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Contact: cacao@cacaojvm.org

   Authors: Christian Thalinger

   Changes:

   $Id: checkjni.java 4695 2006-03-28 14:21:14Z twisti $

*/


public class checkjni {
    public native boolean IsAssignableFrom(Class sub, Class sup);
    public native boolean IsInstanceOf(Object obj, Class clazz);
    public native int     PushLocalFrame(int capacity);

    public static void main(String[] argv) {
        System.loadLibrary("checkjni");

        new checkjni();
    }

    public checkjni() {
        checkIsAssignableFrom();
        checkIsInstanceOf();
        checkPushLocalFrame();
    }

    void checkIsAssignableFrom() {
        p("IsAssignableFrom:");

        Class sub = Integer.class;
        Class sup = Object.class;

        equal(IsAssignableFrom(sup, sup), true);
        equal(IsAssignableFrom(sub, sup), true);
        equal(IsAssignableFrom(sup, sub), false);
    }

    void checkIsInstanceOf() {
        p("IsInstanceOf:");

        Object obj = new Object();
        Object obj2 = new Integer(1);
        Class clazz = Object.class;
        Class clazz2 = Integer.class;

        equal(IsInstanceOf(obj, clazz), true);
        equal(IsInstanceOf(obj2, clazz), true);
        equal(IsInstanceOf(obj, clazz2), false);
    }

    void checkPushLocalFrame() {
        p("PushLocalFrame:");

        equal(PushLocalFrame(100), 0);
    }

    void equal(boolean a, boolean b) {
        if (a == b)
            p("PASS");
        else
            p("FAILED");
    }

    void equal(int a, int b) {
        if (a == b)
            p("PASS");
        else
            p("FAILED");
    }

    void p(String s) {
        System.out.println(s);
    }
}
