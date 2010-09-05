package jbe;

import jbe.kfl.Mast;
import jbe.LowLevel;

public class BenchPredictableKfl extends BenchMark {

	public BenchPredictableKfl() {

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
                LowLevel.preCompile();

                System.out.println("Predictable Kfl");

		BenchMark bm = new BenchPredictableKfl();
                bm.test(100);
	}
			
}

