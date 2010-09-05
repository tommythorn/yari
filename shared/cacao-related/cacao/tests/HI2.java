public class HI2 {
   public   static int max1(int i, int j) {
   
   if (i > j)
     return i;
   else
     return j;
   }

   public static void main(String[] args) {
      int i;
      int j;
      int k;
      for (i=0; i<10; i++) {
	j = (i*2)-5;
	k = max1(i, j);
	}
   }
}
