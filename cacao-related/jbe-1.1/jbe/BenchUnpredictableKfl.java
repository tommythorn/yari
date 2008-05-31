package jbe;

import jbe.kfl.Mast;
import jbe.LowLevel;

class BenchUnpredictableKfl extends BenchMark {

	public BenchUnpredictableKfl() {

		Mast.main(null);
	}

	public int test(int cnt) {

		int i;

		for (i=0; i<cnt; ++i) {
                        int start = LowLevel.getCycleCounter();
			Mast.loop();
                        int end = LowLevel.getCycleCounter();
                        System.out.print("Iteration: ");
                        System.out.println(end - start);
		}

		return i;
	}

	public String getName() {

		return "Kfl";
	}

	public static void main(String[] args) {
                System.out.println("Unpredictable Kfl");

		BenchMark bm = new BenchUnpredictableKfl();
                bm.test(100);
	}
			
}