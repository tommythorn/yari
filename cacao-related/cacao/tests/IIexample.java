class IIexample {
public static void main (String[] args) {
ff();
gg();
}

static void ff() {
II i1 = new IIBB();
i1.foo();
}

static void gg() {
//II i2 = new IICC();
II i2 = new IIBB();  // so unique
i2.foo();
}

}
