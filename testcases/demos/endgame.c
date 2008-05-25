/**
Improved fast endgame solver code, written by
       Gunnar Andersson = gunnar@nada.kth.se
Last modified: July 13, 1999

This update of the old endgame code from the FTP archive at NECI
contains lots of changes that improve code quality and move ordering.
I have not changed Warren's coding style or performed any heavy
loop unrolling - this should be done unless L1 code cache performance
takes a hit. On my system (a Pentium 2), this is not an issue
and it pays off to perform massive unrolling of DoFlips() and CountFlips().
(In a competitive program, this should of course be done.) There are also
a few other speedups that can be achieved at the cost of more code
and less structure. The performance of the code below is now perhaps
10% worse than the low-level code in the program Zebra to which I am the
author, at least for <= 7 empties - for 8-12 empties, Zebra benefits from
using a hash table. Anyway, the code is a bit faster than the old stuff.
Here's a performance comparision for a Pentium 2 / 233MHz (both programs
compiled with gcc -O4 -funroll-loops -Wall -pedantic):

  Warren Smith's code       10.27 seconds
  new code                   4.82 seconds    (53% faster)

(And no, I haven't done any system-specific optimizations, this code
is probably good for most architectures, probably changing the typedefs
of uchar and uint might help on eg. Alphas.)

The code still only is suitable for very few empty squares. The test
positions have at most 12 empties, and this is pretty much the limit.
In fact, hash table lookups should probably be done already for < 10 empties
in most implementations but that would make this minor rehack of the
old code larger than I want it to be. For larger number of empties,
the fastest-first strategy also has to be reconsidered. The details
of this are very much dependent on properties of the evaluation
function, and are hence left as an elementary exercise to the interested
reader.

Changes from Warren Smith's version (in reverse chronological order):
* Parity now cannot be turned off; I removed all the #ifdef's that
  cluttered the code and made it hard to modify. Anybody interested
  in turing parity off (and getting poor performance) can try the
  old endgame code.
* Improved code for positions with 1 empty. (1 % speedup)
* Implemented a pedestrian version of fastest-first, i.e. sort the moves
  in increasing order on the number of opponent responses available.
  (41% speedup)
* The linked list containing squares now also contains parity
  information. (no speedup)
* Removed superfluous check if empties==1 from NoParEndSolve. (no speedup)
* Changed the parity loop in ParEndSolve. (no speedup)
* Only the forward links in the linked list are now update by ParEndSolve
  as the pred fields aren't used down the line. (0.5% speedup)
* Removed killer heuristics from the entire program as it hurt performance
  and cluttered the code. Thus KILLERHEUR1 and KILLERHEUR2 are
  not ignored by the search code. (1% speedup)
* Only the forward links in the linked list are now update by NoParEndSolve
  as the pred fields aren't used down the line. (2% speedup)
* Removed the variable MustPass from ParEndSolve and NoParEndSolve as
  the same information can be deduced from the value of score (no speedup).
* Introduced the symbolic constant INFINITY which holds a value outside
  the range [-64,64] (I took the value 30000 from the old version).
* Unrolled the loop in CountFlips which helps the compiler inline
  CtDrctnlFlips. (8% speed up)
* Unrolled the loop in DoFlips which helps the compiler inline
  DrctnlFlips. (8% speed up)
* main() now returns EXIT_SUCCESS (from <stdlib.h> which is now included)
  when reaching the end instead of exiting without return value.
  Implicit declaration replaced by explicit int main(void).
* stdio.h included - before, printf was used without declaration.
* DoFlips counts flips implicitly by looking at FlipStack.
  (2% speedup)

Old comments:
----------------------------
Fast endgame solver code, written by
       Warren D Smith = wds@research.nec.nj.com
based on code supplied by Jean-Christophe Weill = jcw@seh.etca.fr.
2 Bugs now fixed, thanks to Charles Garrett and Mark Brockington.
Date: Wed Nov 16 07:11:14 EST 1994

See the testdriver routine for how to call it. PrepareToSolve() and
EndSolve() are the routines to export. Everything else is local.
Compile with gcc -O2 thisfile.c and run.

I have translated Jcw's code into English and made a few algorithm mods:
   The most important enhancements are the WINNER_GETS_EMPTIES scoring
mode (which will slow you down by a factor of 1.05), and the use of
parity to help do move ordering, which results in a speedup factor (on
the 112-position test set) of 1.30 with USE_PARITY=2. The flip
stack is simplified.

This code is designed to solve positions with <=10 empties (although
in principle it will work for any number of empties up to the number
of bits in a uint (32 for me). **/
#define MAXEMPTIES 32
/** It is plain alphabeta, no transposition table.  It can be used for
WLD solve by setting alpha=-1 and beta=1 (in which case
WINNER_GETS_EMPTIES could be turned off to get a tiny speedup...) or
for full solve with alpha=-64, beta=64.  It uses a fixed preference
ordering of squares. This is not such a bad thing to do when <=10
empties, where the demand for speed is paramount. When USE_PARITY=X is
turned on (X>0), it also uses parity to help with move ordering,
specifically it will consider all moves into odd regions before
considering any moves into even regions, in situations with more than
X empties.  **/
#define USE_PARITY 4
/** In situations with <=X empties, parity will not be used. I also
put in response-based killers, which yield a speedup of 1.01. **/
#define KILLERHEUR 0
/** Putting killers, or even just killer data-gathering, in the
deeper (parity using) part of the search is experimentally not worth it. **/
#define KILLERHEUR2 0
/** If WINNER_GETS_EMPTIES is turned on it will do scoring using the
winner-gets-empties convention - which never changes who won, but can
change the final score. **/
#define WINNER_GETS_EMPTIES 1
/** Appears to work... at least, it produces the correct game values
(-12,-6,28,-8) for the first 4 test positions and also
for the last 12 (trivial and artificial) test positions. Has now been
more extensively tested.

Unix time results: 33 MHZ IP12 Processor, MIPS R2000A/R3000, gcc -g -O2
  (with WINNER_GETS_EMPTIES=0, FULLSOLVE=2,
   and the 112 test positions in the driver,
   only 100 of which take time [each of the 100 is 12 empties])
USE_PARITY=0: 85.6u 0.4s 1:28 97%
USE_PARITY=1: 69.0u 0.4s 1:13 95%
USE_PARITY=2: 65.8u 0.3s 1:08 97%
USE_PARITY=3: 66.8u 0.3s 1:09 97%
USE_PARITY=4: 67.8u 0.3s 1:11 95% *
USE_PARITY=5: 70.9u 0.5s 1:14 95%
USE_PARITY=6: 72.3u 0.4s 1:16 95% 
  with WINNER_GETS_EMPTIES=1, USE_PARITY=2, FULLSOLVE=2: 69.2u 0.5s 1:13 95%
  with WINNER_GETS_EMPTIES=0, USE_PARITY=2, FULLSOLVE=1: 15.3u 0.1s 0:16 91%
  with WINNER_GETS_EMPTIES=1, USE_PARITY=2, FULLSOLVE=0: 11.8u 0.1s 0:12 92%
  with WINNER_GETS_EMPTIES=0, USE_PARITY=2, FULLSOLVE=0: 11.8u 0.1s 0:12 92%
Node rate: (where each invocation of EndSolve is a "node") 37000-39000/sec.

(On morgoth, 150 MHZ IP19 MIPS R4400, get 147000-149000/sec. 3.8 times faster.)

Later note: some new code mods have now speeded it up about 1.21,
further, beyond the experiments above, and the best value of USE_PARITY
is now 4. Thus the *'d line above now is
    53.0u 0.1s 0:52 100%.
Using TRIES=2 proves that an additional 1.09 speedup could be obtained
by initializing the killer tables to something sensible before searching.

So 175000/sec on morgoth now. Possibly changing uchars
to uints or vice versa in certain places will lead to even more
speedups, depending on the machine. I believe the current char/int combination
is, however, optimal on *my* machine.  In place of the fixed order
best2worst[] you could use some order which (based on past experience
searching positions closely related to this one, e.g. Schaeffer's history
heuristic) you think will be good for this particular position. 
In place of the killertables being initially set to all 0's, you
could initially set them to be responses you think will be good in this
particular position. All this could lead to speedups of an
additional 1.5 or so.
********************/

/* In positions with <= FASTEST_FIRST empty, fastest first is disabled. */
#define FASTEST_FIRST 7

#define INFINITY 30000

#include <stdio.h>
#include <stdlib.h>

#define double int // BENCH HACK: double is slow and not needed below
/* #define WINDOWS_TIMING */

#ifdef WINDOWS_TIMING
#include <time.h>
#include <windows.h>

double
get_real_time( void ) {
  int ticks;

  ticks = GetTickCount();

  return ticks / ((double) CLK_TCK);
}
#endif

/* Inside this fast endgame solver, the board is represented by
* a 1D array of 91 uchars board[0..90]:
   ddddddddd
   dxxxxxxxx
   dxxxxxxxx
   dxxxxxxxx
   dxxxxxxxx
   dxxxxxxxx
   dxxxxxxxx
   dxxxxxxxx       where A1 is board[10], H8 is board[80].
   dxxxxxxxx       square(a,b) = board[10+a+b*9] for 0<= a,b <=7.
   dddddddddd   
where d (dummy) squares contain DUMMY, x are EMPTY, BLACK, or WHITE: */
#define WHITE 0
#define EMPTY 1
#define BLACK 2
#define DUMMY 3
#define uchar unsigned char
#define schar signed char
#define uint unsigned int
uchar board[91];
/* Also there is a doubly linked list of the empty squares.
EmHead points to the first empty square in the list (or NULL if none).
The list in maintained in a fixed best-to-worst order. */
struct EmList
{
    int square;
    int hole_id;
    struct EmList *pred;
    struct EmList *succ;
} EmHead, Ems[64];
#define NULL 0
/* Also, and finally, each empty square knows the region it is in
and knows the directions you can flip in via some bit masks.
There are up to 32 regions. The parities of the regions are in
the RegionParity bit vector: */
uint HoleId[91];
uint RegionParity;

/* The 8 legal directions: */
const schar dirinc[] = {1, -1, 8, -8, 9, -9, 10, -10, 0};
/* The bit mask for direction i is 1<<i */

/* Bit masks for the directions squares can flip in,
* for example dirmask[10]=81=64+16+1=(1<<6)+(1<<4)+(1<<0)
* hence square 10 (A1) can flip in directions dirinc[0]=1,
* dirinc[4]=9, and dirinc[6]=10: */
uchar dirmask[91] = {
0,0,0,0,0,0,0,0,0,
0,81,81,87,87,87,87,22,22,
0,81,81,87,87,87,87,22,22,
0,121,121,255,255,255,255,182,182,
0,121,121,255,255,255,255,182,182,
0,121,121,255,255,255,255,182,182,
0,121,121,255,255,255,255,182,182,
0,41,41,171,171,171,171,162,162,
0,41,41,171,171,171,171,162,162,
0,0,0,0,0,0,0,0,0,0
};

/* fixed square ordering: */
/* jcw's order, which is the best of 4 tried: */
int worst2best[64] =
{
/*B2*/      20 , 25 , 65 , 70 ,
/*B1*/      11 , 16 , 19 , 26 , 64 , 71 , 74 , 79 ,
/*C2*/      21 , 24 , 29 , 34 , 56 , 61 , 66 , 69 ,
/*D2*/      22 , 23 , 38 , 43 , 47 , 52 , 67 , 68 ,
/*D3*/      31 , 32 , 39 , 42 , 48 , 51 , 58 , 59 ,
/*D1*/      13 , 14 , 37 , 44 , 46 , 53 , 76 , 77 ,
/*C3*/      30 , 33 , 57 , 60 ,
/*C1*/      12 , 15 , 28 , 35 , 55 , 62 , 75 , 78 ,
/*A1*/      10 , 17 , 73 , 80 , 
/*D4*/      40 , 41 , 49 , 50
};

uchar * GlobalFlipStack[2048];
uchar **FlipStack = &(GlobalFlipStack[0]);

/* sq is a pointer to the square the move is to. */
/* inc is the increment to go in some direction. */
/* color is the color of the mover */
/* oppcol = 2-color is the opposite color. */
/* FlipStack records locations of flipped men so can unflip later. */
/* This routine flips in direction inc and returns count of flips it made: */
inline
void
DrctnlFlips( uchar *sq, int inc, int color, int oppcol )
{
   uchar *pt = sq + inc;
   if(*pt == oppcol){
      pt += inc;
      if(*pt == oppcol){
          pt += inc;
          if(*pt == oppcol){
              pt += inc;
              if(*pt == oppcol){
                  pt += inc;
                  if(*pt == oppcol){
                      pt += inc;
                      if(*pt == oppcol){
                          pt += inc;
                      }
                  }
              }
          }
      }
      if(*pt == color){
	pt -= inc;
          do{
             *pt = color;
             *(FlipStack++) = pt;
	     pt -= inc;
          }while( pt != sq);
      }
   }
}

/* Do all flips involved in making a move to square sqnum of board,
 * and return their count. */
int
DoFlips( uchar *board, int sqnum,
         int color, int oppcol )
{
   int j=dirmask[sqnum];
   uchar **OldFlipStack = FlipStack;
   uchar *sq;
   sq = sqnum + board;
   if ( j & 128 )
     DrctnlFlips( sq, dirinc[7], color, oppcol );
   if ( j & 64 )
     DrctnlFlips( sq, dirinc[6], color, oppcol );
   if ( j & 32 )
     DrctnlFlips( sq, dirinc[5], color, oppcol );
   if ( j & 16 )
     DrctnlFlips( sq, dirinc[4], color, oppcol );
   if ( j & 8 )
     DrctnlFlips( sq, dirinc[3], color, oppcol );
   if ( j & 4 )
     DrctnlFlips( sq, dirinc[2], color, oppcol );
   if ( j & 2 )
     DrctnlFlips( sq, dirinc[1], color, oppcol );
   if ( j & 1 )
     DrctnlFlips( sq, dirinc[0], color, oppcol );

   return FlipStack - OldFlipStack;
}

/* For the last move, we compute the score without updating the board: */
inline
int
CtDrctnlFlips( uchar *sq, int inc, int color, int oppcol )
{
   uchar *pt = sq + inc;
   if(*pt == oppcol){
      int count = 1;
      pt += inc;
      if(*pt == oppcol){
          count++;                /* 2 */
          pt += inc;
          if(*pt == oppcol){
              count++;                /* 3 */
              pt += inc;
              if(*pt == oppcol){
                  count++;        /* 4 */
                  pt += inc;
                  if(*pt == oppcol){
                      count++;        /* 5 */
                      pt += inc;
                      if(*pt == oppcol){
                          count++;        /* 6 */
                          pt += inc;
                      }
                  }
              }
          }
      }
      if(*pt == color) return count;
   }

   return 0;
}

int
CountFlips( uchar *board, int sqnum,
         int color, int oppcol )
{
   int ct=0;
   int j=dirmask[sqnum];
   uchar *sq;
   sq = sqnum + board;
   if ( j & 128 )
     ct += CtDrctnlFlips( sq, dirinc[7], color, oppcol );
   if ( j & 64 )
     ct += CtDrctnlFlips( sq, dirinc[6], color, oppcol );
   if ( j & 32 )
     ct += CtDrctnlFlips( sq, dirinc[5], color, oppcol );
   if ( j & 16 )
     ct += CtDrctnlFlips( sq, dirinc[4], color, oppcol );
   if ( j & 8 )
     ct += CtDrctnlFlips( sq, dirinc[3], color, oppcol );
   if ( j & 4 )
     ct += CtDrctnlFlips( sq, dirinc[2], color, oppcol );
   if ( j & 2 )
     ct += CtDrctnlFlips( sq, dirinc[1], color, oppcol );
   if ( j & 1 )
     ct += CtDrctnlFlips( sq, dirinc[0], color, oppcol );
   return(ct);
}

/* Sometimes we only want to know if a move is legal, not how
   many discs it flips. */
inline
int
AnyDrctnlFlips( uchar *sq, int inc, int color, int oppcol )
{
   uchar *pt = sq + inc;
   if(*pt == oppcol){
      pt += inc;
      if(*pt == oppcol){
          pt += inc;
          if(*pt == oppcol){
              pt += inc;
              if(*pt == oppcol){
                  pt += inc;
                  if(*pt == oppcol){
                      pt += inc;
                      if(*pt == oppcol){
                          pt += inc;
                      }
                  }
              }
          }
      }
      if(*pt == color) return 1;
   }

   return 0;
}
int
AnyFlips( uchar *board, int sqnum,
	  int color, int oppcol )
{
   int any=0;
   int j=dirmask[sqnum];
   uchar *sq;
   sq = sqnum + board;
   if ( j & 128 )
     any += AnyDrctnlFlips( sq, dirinc[7], color, oppcol );
   if ( j & 64 )
     any += AnyDrctnlFlips( sq, dirinc[6], color, oppcol );
   if ( j & 32 )
     any += AnyDrctnlFlips( sq, dirinc[5], color, oppcol );
   if ( j & 16 )
     any += AnyDrctnlFlips( sq, dirinc[4], color, oppcol );
   if ( j & 8 )
     any += AnyDrctnlFlips( sq, dirinc[3], color, oppcol );
   if ( j & 4 )
     any += AnyDrctnlFlips( sq, dirinc[2], color, oppcol );
   if ( j & 2 )
     any += AnyDrctnlFlips( sq, dirinc[1], color, oppcol );
   if ( j & 1 )
     any += AnyDrctnlFlips( sq, dirinc[0], color, oppcol );
   return(any);
}


/* Call this right after FlipCount=DoFlips() to Undo those flips! */
inline
void
UndoFlips( int FlipCount, int oppcol )
{/************************************************************************
** This is functionally equivalent to the simpler but slower code line:  *
**   while(FlipCount){ FlipCount--;  *(*(--FlipStack)) = oppcol; }       *
*************************************************************************/
   if(FlipCount&1){
      FlipCount--;
      * (*(--FlipStack)) = oppcol;
   }
   while(FlipCount){
      FlipCount -= 2;
      * (*(--FlipStack)) = oppcol;
      * (*(--FlipStack)) = oppcol;
   }
}

inline
uint minu(uint a, uint b)
{
   if(a<b) return a;
   return b;
}

/* Set up the data structures, other than board array,
 * which will be used by solver. Since this routine consumes
 * about 0.0002 of the time needed for a 12-empty solve,
 * I haven't worried about speeding it up. */
void
PrepareToSolve( uchar *board )
{
   int i,sqnum;
   uint k;
   struct EmList *pt;
   int z;
   /* find hole IDs: */
   k = 1;
   for(i=10; i<=80; i++){
      if(board[i]==EMPTY){
         if( board[i-10]==EMPTY ) HoleId[i] = HoleId[i-10];
         else if( board[i-9]==EMPTY ) HoleId[i] = HoleId[i-9];
         else if( board[i-8]==EMPTY ) HoleId[i] = HoleId[i-8];
         else if( board[i-1]==EMPTY ) HoleId[i] = HoleId[i-1];
         else{ HoleId[i] = k; k<<=1; }
      }
      else HoleId[i] = 0;
   }
#define MAXITERS 1
   /* In some sense this is wrong, since you
    * ought to keep doing iters until reach fixed point, but in most
    * othello positions with few empties this ought to work, and besides,
    * this is justifiable since the definition of "hole" in othello
    * is somewhat arbitrary anyway. */
   for(z=MAXITERS; z>0; z--){
      for(i=80; i>=10; i--){
         if(board[i]==EMPTY){
            k = HoleId[i];
            if( board[i+10]==EMPTY ) HoleId[i] = minu(k,HoleId[i+10]);
            if( board[i+9]==EMPTY ) HoleId[i] = minu(k,HoleId[i+9]);
            if( board[i+8]==EMPTY ) HoleId[i] = minu(k,HoleId[i+8]);
            if( board[i+1]==EMPTY ) HoleId[i] = minu(k,HoleId[i+1]);
         }
      }
      for(i=10; i<=80; i++){
         if(board[i]==EMPTY){
            k = HoleId[i];
            if( board[i-10]==EMPTY ) HoleId[i] = minu(k,HoleId[i-10]);
            if( board[i-9]==EMPTY ) HoleId[i] = minu(k,HoleId[i-9]);
            if( board[i-8]==EMPTY ) HoleId[i] = minu(k,HoleId[i-8]);
            if( board[i-1]==EMPTY ) HoleId[i] = minu(k,HoleId[i-1]);
         }
      }
   }
   /* find parity of holes: */
   RegionParity = 0;
   for(i=10; i<=80; i++){
      RegionParity ^= HoleId[i];
   }
   /* create list of empty squares: */
   k = 0;
   pt = &EmHead;
   for(i=60-1; i>=0; i--){
      sqnum = worst2best[i];
      if( board[sqnum]==EMPTY ){
         pt->succ = &(Ems[k]);
         Ems[k].pred = pt;
         k++;
         pt = pt->succ;
         pt->square = sqnum;
	 pt->hole_id = HoleId[sqnum];
      }
   }
   pt->succ = NULL;
   if(k>MAXEMPTIES) abort(); /* better not have too many empties... */
}

int
NoParEndSolve (uchar *board, double alpha, double beta, 
   int color, int empties, int discdiff, int prevmove )
{
   int score = -INFINITY;
   int oppcol = 2-color;
   int sqnum,j,ev;
   struct EmList *em, *old_em;
   for(old_em = &EmHead, em = old_em->succ; em!=NULL;
       old_em = em, em = em->succ){
      /* go thru list of possible move-squares */
      sqnum = em->square;
      j = DoFlips( board, sqnum, color, oppcol );
      if(j){ /* legal move */
          /* place your disc: */
          *(board+sqnum) = color;
          /* delete square from empties list: */
	  old_em->succ = em->succ;
          if(empties==2){ /* So, now filled but for 1 empty: */
             int j1;
             j1 = CountFlips( board, EmHead.succ->square, oppcol, color);
             if(j1){ /* I move then he moves */
                ev = discdiff + 2*(j-j1);
             }
             else{ /* he will have to pass */
                j1 = CountFlips(board, EmHead.succ->square, color, oppcol);
		ev = discdiff + 2*j;
                if(j1){ /* I pass then he passes then I move */
		  ev += 2 * (j1 + 1);
                }
                else{ /* I move then both must pass, so game over */
#if WINNER_GETS_EMPTIES
		   if ( ev >= 0 )
		     ev += 2;
#else
                   ev++;
#endif

                }
             }
          }
          else{
             ev = -NoParEndSolve(board, -beta, -alpha, 
                      oppcol, empties-1, -discdiff-2*j-1, sqnum);
          }
          UndoFlips( j, oppcol );
          /* un-place your disc: */
          *(board+sqnum) = EMPTY;
          /* restore deleted empty square: */
	  old_em->succ = em;

          if(ev > score){ /* better move: */
             score = ev;
             if(ev > alpha){
                alpha = ev;
                if(ev >= beta){ /* cutoff */
                   return score;
                }
             }
          }
       }
    }
    if(score == -INFINITY){  /* No legal move */
       if(prevmove == 0){ /* game over: */
#if WINNER_GETS_EMPTIES
          if(discdiff>0) return discdiff+empties;
          if(discdiff<0) return discdiff-empties;
          return 0;
#else
          return discdiff;
#endif
       }
       else /* I pass: */ return
          -NoParEndSolve( board, -beta, -alpha, oppcol, empties, -discdiff, 0);
    }
    return score;
}

int
ParEndSolve (uchar *board, double alpha, double beta, 
   int color, int empties, int discdiff, int prevmove )
{
   int score = -INFINITY;
   int oppcol = 2-color;
   int sqnum,j,ev;
   struct EmList *em, *old_em;
   uint parity_mask;
   int par, holepar;

   for ( par = 1, parity_mask = RegionParity; par >= 0;
	 par--, parity_mask = ~parity_mask ) {

   for(old_em = &EmHead, em=old_em->succ; em!=NULL;
       old_em = em, em = em->succ){
      /* go thru list of possible move-squares */
      holepar = em->hole_id;
      if ( holepar & parity_mask ) {
      sqnum = em->square;
      j = DoFlips( board, sqnum, color, oppcol );
      if(j){ /* legal move */
          /* place your disc: */
          *(board+sqnum) = color;
          /* update parity: */
          RegionParity ^= holepar;
          /* delete square from empties list: */
	  old_em->succ = em->succ;
          if(empties<=1+USE_PARITY)
             ev = -NoParEndSolve(board, -beta, -alpha, 
                   oppcol, empties-1, -discdiff-2*j-1, sqnum);
          else
             ev = -ParEndSolve(board, -beta, -alpha, 
                   oppcol, empties-1, -discdiff-2*j-1, sqnum);
          UndoFlips( j, oppcol );
          /* restore parity of hole */
          RegionParity ^= holepar;
          /* un-place your disc: */
          *(board+sqnum) = EMPTY;
          /* restore deleted empty square: */
	  old_em->succ = em;

          if(ev > score){ /* better move: */
             score = ev;
             if(ev > alpha){
                alpha = ev;
                if(ev >= beta){ 
                   return score;
	       }
	    }
	 }
       }
    }}
    }

    if(score == -INFINITY){  /* No legal move found */
       if(prevmove == 0){ /* game over: */
#if WINNER_GETS_EMPTIES
          if(discdiff>0) return discdiff+empties;
          if(discdiff<0) return discdiff-empties;
          return 0;
#else
          return discdiff;
#endif
       }
       else /* I pass: */ return
          -ParEndSolve( board, -beta, -alpha, oppcol, empties, -discdiff, 0);
    }
    return score;
}


int
count_mobility( uchar *board, int color ) {
  int oppcol = 2 - color;
  int mobility;
  int square;
  struct EmList *em;

  mobility = 0;
  for ( em = EmHead.succ; em != NULL; em = em->succ ) {
    square = em->square;
    if ( AnyFlips( board, square, color, oppcol ) )
      mobility++;
  }

  return mobility;
}


int
FastestFirstEndSolve( uchar *board, double alpha, double beta, 
		      int color, int empties, int discdiff,
		      int prevmove ) {
  int i, j;
  int score = -INFINITY;
  int oppcol = 2 - color;
  int sqnum, ev;
  int flipped;
  int moves, mobility;
  int best_value, best_index;
  struct EmList *em, *old_em;
  struct EmList *move_ptr[64];
  int holepar;
  int goodness[64];

  moves = 0;
  for ( old_em = &EmHead, em = old_em->succ; em != NULL;
	old_em = em, em = em->succ ) {
    sqnum = em->square;
    flipped = DoFlips( board, sqnum, color, oppcol );
    if ( flipped ) {
      board[sqnum] = color;
      old_em->succ = em->succ;
      mobility = count_mobility( board, oppcol );
      old_em->succ = em;
      UndoFlips( flipped, oppcol );
      board[sqnum] = EMPTY;
      move_ptr[moves] = em;
      goodness[moves] = -mobility;
      moves++;
    }
  }

  if ( moves != 0 ) {
    for ( i = 0; i < moves; i++ ) {
      best_value = goodness[i];
      best_index = i;
      for ( j = i + 1; j < moves; j++ )
	if ( goodness[j] > best_value ) {
	  best_value = goodness[j];
	  best_index = j;
	}
      em = move_ptr[best_index];
      move_ptr[best_index] = move_ptr[i];
      goodness[best_index] = goodness[i];

      sqnum = em->square;
      holepar = em->hole_id;
      j = DoFlips( board, sqnum, color, oppcol );
      board[sqnum] = color;
      RegionParity ^= holepar;
      em->pred->succ = em->succ;
      if ( em->succ != NULL )
	em->succ->pred = em->pred;
      if ( empties <= FASTEST_FIRST + 1 )
	ev = -ParEndSolve( board, -beta, -alpha, oppcol, empties - 1,
			   -discdiff - 2 * j - 1, sqnum);
      else
	ev = -FastestFirstEndSolve( board, -beta, -alpha, oppcol,
				    empties - 1, -discdiff - 2 * j - 1,
				    sqnum);
      UndoFlips( j, oppcol );
      RegionParity ^= holepar;
      board[sqnum] = EMPTY;
      em->pred->succ = em;
      if ( em->succ != NULL )
	em->succ->pred = em;

      if ( ev > score ) { /* better move: */
	score = ev;
	if ( ev > alpha ) {
	  alpha = ev;
	  if ( ev >= beta )
	    return score;
	}
      }
    }
  }
  else {
    if ( prevmove == 0 ) {
      if ( discdiff > 0 )
	return discdiff + empties;
      if ( discdiff < 0 )
	return discdiff - empties;
      return 0;
    }
    else  /* I pass: */
      score = -FastestFirstEndSolve( board, -beta, -alpha, oppcol,
				     empties, -discdiff, 0);
  }

  return score;
}



/* The search itself. Assumes relevant data structures have been set up with
 * PrepareToSolve.
 * color is the color on move. Discdiff is color disc count - opposite
 * color disc count. The value of this at the end of the game is returned.
 * prevmove==0 if previous move was a pass, otherwise non0.
 * empties>0 is number of empty squares.
*******************/

int
EndSolve (uchar *board, double alpha, double beta, 
   int color, int empties, int discdiff, int prevmove )
{
  if ( empties > FASTEST_FIRST )
    return FastestFirstEndSolve(board,alpha,beta,color,empties,discdiff,prevmove);
  else {
   if(empties <= (2>USE_PARITY ? 2 : USE_PARITY) )
      return NoParEndSolve(board,alpha,beta,color,empties,discdiff,prevmove);
   else 
   return ParEndSolve(board,alpha,beta,color,empties,discdiff,prevmove);
  }
}

/*** ------------ snip here to get rid of test driver ----------- ***/
/* test driver. Solves 112 positions, 100 of them with 12 empties,
** and 12 of them trivial. */
#define FULLSOLVE 2
/* 2 for full, make 1 if just WLD solve, 0 if WD solve. */

#define MAXTRIES 1
/* solves each position MAXTRIES times. Note if 2 times is faster than   *
*  2*(1 time) then that proves the killer tables helped...               */

int main( void ){
   int tries,val,emp,wc,bc,i,j,k,x,y;
   char bds[112][65] = {
"..wwwww.b.wwbw..bbwbbwwwbwbbbwwwbbwbbwwwbbbwbwww..bbwbw....bbbbw",
"wbbw.b...wwbwww.wwwwbwwwwwwbwbbbwwwwbwb.wwwwwbbb..wwbbb..wwwww..",
"..w.bb.bw.wbbbbbwwwwbbbbwbwwwwbbwbbbwwbwwwbbbbbww.wbbbbw......bw",
"bwwwwww..bwwwww..bbbwwww.bbbbbww.bbbbwww.bbbbbww..bbbbw...bbbbbw",
"b.wbw....bwwww.bbbbbbbbbbbbbbbbbbbwbbbbbbbbwwbbbb.wwww....w.bbbb",
"..bwb..b.bbwwwwwbbbwbbwbbwwbbbwbbwwwbwbbb.wwwwbbb.wwwb.b...wbbb.",
"...bwwbb..bbbwwb.bbbbbwwbbwbbbb.bbbwbbbb.bbbwwbb..bbbbbb..bbbbwb",
"..bbbbwb..bbbwbb.bbwwbbbwwwwbbbb.wwbbbbb.wwbbwbb.bbwww...bwwwww.",
"..b.....wwbb.b..wwwbbbbbbwwwbbwwbbwbbwwwbbbwwwwwbwbbwb..bwwwwwww",
"b.w.b...bb.bbw.wbbbbbbwwbbbbbwwwbwwwwwwwbwbwwww.bbwwww.b..wwwww.",
"b..b....b.bbbb.bbbwwwwbbbbbbwbbbbwbwbbbbbwbwbwbbbwwwww..bwwwww..",
"wbbb.ww.wbbbbw.bwbbwwb.bwbbwwbwbwwbwwww.wwbwwwwwwbwwww....ww..w.",
"bbbbbbb...wwww.bbbbbbwwb.bwbwwwbbbbwwwwb.bbbwwwb..wwbww...wwwww.",
"b.w.b...bb.bbw.wbbbbbbwwbbbbbwwwbwwwwwwwbwbwwww.bbwwww.b..wwwww.",
"bbbbbbb...wwww.bbbbbbwwb.bwbwwwbbbbwwwwb.bbbwwwb..wwbww...wwwww.",
"b..b..w.bbbbbw.bbwbwwwbbbwbbwbbbbwbwbbbbbwbbbwbbbbbbww..b.b.ww..",
"b..b....b.bbbb.bbbwwwwbbbbbbwbbbbwbwbbbbbwbwbwbbbwwwww..bwwwww..",
"...bbbb...bbbb.wbbbbbww.wbwbwww.wbbbbwwwbbbbbbww.bbbbbbw.wwwwww.",
"..bbb.....bbbw.wwwwwbwwwbwwbbbbwwwbwwbbw.bwwbwbwb.wwbbbw..wwwwbw",
"w..bbbb.bbbbbbb.wbwbwwbbwwbbwbbbwwbbwbbbw.wwwwbb..wwww.b..wwww..",
"..bbbbb.wwbbbbb..wwbbbbwbbwwwbbwbbwwwbbwbbbbbwbww.wwwwww.....w.w",
".bbb....w.bbbb.wwbbbbbbbwbbwbbbbwbwbwwbbwwbbbbwbw.bbbwww...bww.w",
"..wwwww.bbwwww.bbbbwwwbbbwbbwbwbbwbbwbbbbbwbbbbbb.wwww......www.",
"w..bbb..bbbbbb..wbwbwbbbwbbwwbbbwbwbwbbbwbbwwwbb.bwwww.b..wwww..",
".bbbbbbb..wwbwb.wwwwwbww.wbwbwwb.bwbbwbb.wbbbbbb...bbbbb..wwwwww",
"..bbbbbw..bbbbww..bbbwbw..bbwbbw.wbwwbbwwwbbwwwwwwbbbb.bbbbbbb..",
".wwwww...wwbbbb.wwwwbbbbwwwwbwb.wwwwwww.wbwwwwwwbbbwww..wwww..w.",
"b.w.b...bb.bbb..bbbwwwwwbbbbbwwwbwbbwwbwbbwwbwwwbwwwwwwww....wbw",
"...bbbb...bbbb.wbbbbbww.wbwbwww.wbbbbwwwbbbbbbww.bbbbbbw.wwwwww.",
".wbbbb.w..bbbb.w.wbwwwbw.bbwwwww.bbwwwbw.bwbwbww.bbbbww.wbwwwww.",
"..wbbw.w..wwwwwbwwwwwwbbwwwbwbwbwwbwbwwbwbbbwbbbw.bbbbbb..bb....",
"..bbbbb..bbbbbbbbbbwbbbbwbbbwbbbbbbbbwbb.bbbwww..bbbbwww....bbw.",
"..w..bww...wbbbb..wwbbbwbbbbbbwwbbbbwbwwbwbbwwwwbbwwwww.bbbbb.w.",
".wwwww..b.wbbw.w.bbwwbwwwwbbwwb.wwwbwwbbwwbwbwbb..bbwbb...bbbbbb",
"....w.....wwww.wwwwwbbbbwwwbwbbbwwwbwwbbwwbwwbwb.bbbbbbw.wwwwwww",
".ww.b.....wwbb..wbbwwbbbwbbbwwbbwbbbbbwbwbbbbwbb.bbbwb.b.bbbbbbb",
"..wbbbbww.bbwwwwwbbbwwwwwbbwwbwwwwwwwwwww.wbbwwww.w..www...bbb..",
".wbbbb.w..bbbb.w.wbwwwbw.bbwwwww.bbwwwbw.bwbwbww.bbbbww.wbwwwww.",
"..bbbb..w.bww...wwbbww..wbbwbwwwwbbbwwwwwbbbbwbw.bbwbbbw.bbbbbbw",
".bb.w....bbbww.bwbwbbwbbwwbwwbbbwwbwwwb.wwwbbwww.wwwwwww..bwwww.",
".....w....wwwwwwbbwbbbwb.bwwwwwbbbbwwbwbbbwbbwbbbbbbbbbbwbbbbb..",
"w..bww..bwwbbbbbbbbwbbbbbbbbwbbb..bwwwb...wwbwww.wwwwwww.wwwwww.",
"wwwwwbw...wwwww.bbwbbbbb.bbwwbbbbbbbbbbb.bwwwbbbb.wwww....wwwww.",
"..w.wb..wwbwww..wwbbwbw.wbbwbbw.wbbbwbwwwwbwwwbwwbbbbbbwb.bb..bw",
"..wbbw..wwwbbbbbwwbbbbbbwwwbbbwbwwwbwwww.wbbwb.w.bbbbw...b.wwww.",
"..wbbb.....b.bwwwwbbwbwwwwbbwwww.wbwwbww.wwbbwwb.wwwwwwb.wwwwwww",
"b.wwwww.bbwwwwb.bwbwwbbb.bwbbwbb.wbwbbbbw.bwbbbw...wwbbw..bw.wbw",
"..bwww....bbbb...wbwbb.wwwbwwwwwwwbbwbwwwwbbbwbwwwwbbbbw..wbbbbw",
"bbbbbb...bwbbbbbbbbwwbbwbbwbbbww.bwbbbw..bwbwbw...wwbwb...wwwwwb",
"w.w.....w.wwwb..wwwwbwwbbwbbbwwbbwbwwbwbbbwwbbbbbwwbww.bwwwwww..",
"....b.wb..bbbbbb..bbbbbb..bbbbw.wwbbbwwwwbbbbwwwbbbwwwwwbwwwwwww",
".bbbbbb.b.bbbb.bbbwwwwbbbbwwbwwbbbwbwbwb.wbwwwbbw.wwww.b...wbw..",
".wwwwww...wbbw..w.bwwbw..bbbwbwwbbbwbbwwbbwbbbbw.wbbbbbw..wbbbbw",
"..wbbw...wwwwwbbbbwbwwwbbbbwbbwbbbbbbbwbbbbbbwbb..bbbb.b..bbbb..",
"..wbbw..wwwbbbbbwwbbbbbbwwwbbbwbwwwbwwww.wbbwb.w.bbbbw...b.wwww.",
"b..www...bwwwwwwbwbbbbwwbwbbbwbwbbbbbbbwbbbbbbbwb..bbbbw...w..bw",
".b.www....bbbb...wwbbb.wwwwwbwwwwwwbwbwwwwbbbwbwwwwbbbbw..wbbbbw",
".bbbbb...wwwwbbbwwwbwbb.bwbwwbbwbbwbwbbwbbbbbbww...bbbww....wbbw",
"..bbbb.b..bwbbb.bbbbbbbbbbbbbbbbbbbbwbbbwbbbbbbb..bbbb...wwwwww.",
".wbw.....bbbwb..bwbwbw...wwwwwwbwwwwbwbbwwwwwbbbbwbwbbbb.bwwwwwb",
".wwwww....wbbw..bbbbbww.bbbwbww.bbbwwbwwbbbbwbww.wbbbbbw..bbbbbw",
"wwwbbbbwbbbbbbbwwbwbwwww.wbwwwbwwwwwwbwwb.bbbwww...bbww......bw.",
".b.wwww...bbbw...wwbwb.wwwwwbbb.wwwbwbbbwwbbbwbbwwwbbbbb..wbbbbw",
"..b.b.w.w.bbbb.b.bbbwbbbbbwbbwbbbwwwbwwbbwwwwbwbb.wwwwbb..wwww.b",
".wwwww...bbbbw.bbwbwwwbbbwwbwbbbbwbwbbb.wwwbwb.w.wwbbw...wwwwww.",
"..bbbw...bbbbb..bwwbbbbbbbwwbbbbbwbbwwwwbbwbwwwwb.bwbww..b.wwww.",
"..bbbbb.w.bbwb..wbbwbww.bbbbwww.bwwwwww.bwwwwwwwb.wwwww..bwwwwww",
"..w.wb....wwwb.wbbwwwbwwbwwwbwwwbbbwwbwwwwwbwwbw..wwbbbb..wwwwww",
"..bbbbbbw.bbbb.bwwbwbwwbwbwbwwwbwwbbwbwbwwwwwwbb...w.bwb....bbww",
"...bw....bbwwb.bwwbwwbbbwwwbwbbb.wwbbbbbbwwbbbwb..wwwwwb.bwwwbbb",
"..wbwwwbwbbbbwwbwbbbwbwbwwwwbwwbwbwbwbwbwwbwwwbbw..bww.b.....w..",
"..bbw..w..bwwwbbwwbbwwwbwbbwwwwwwbwwwwwwwwwwwww.wbwww...wwwwww..",
"..wwwww..bbbww.wbbbwwwwwbbbwwwbwbbwbwwwwbwbbbwwwbbbbbb..b.b...b.",
".bbbbbb...bbwb..wwbwbwwbwwwbwwwwwwwwbbwb.wwwbbw..wwbwww...bwwwwb",
".bbb..wwb.bwbwbbwbwbwwbbwbbwwwbwwbwwbw..wwwwwww.wbwww...wwwwww..",
"..wwww....wwwwb.bbwwwbbwbwwwbwbwwwwwwwbbbbbwbwbw.bbbww..bwwwww..",
".bbbbbbb.bbw.wb.wbbwwbwbwbbbbw.wwbwbbww.wbbwwb..wbbbbbb..wwwww..",
"wbbbwb...bbbbw..wbwwwwwbwwbwwwbbwwwwwbwbwwwbwwbb..wbww.b..wwww..",
"..wbb...bbwbbb..bbbwwwbwbbwbbbbbbbbwbwbb.bbbwbbbwwbwbbb..wwbbw..",
"bbbbbbb..bwwwb.w.wbbbbbw.wwbbwbw.wwwwbbw..wwbwbw...wbbbw.wwwwwww",
"b.w..w.b.bwwwwww.bbwbbwwbbbbbwbwwbwwwwww..bbbbbw..bbbbbw..bbbbbw",
".bbbbb..b.bbbbw.bbwbbwb.bwwwbbbbbwwbwbbwbwbbbbb.bbbwww..bb...www",
"wwwwwww..wbbbw.w.wwbbbbw.wbwbbbwwwwwbwwwb.wwbwbw..wwbbbw...wb.bw",
".bbbwww..bbwwwwwwbwwwwwwwwbbbwwwwbwbbbwwwwbbwbbbw.bbb.....bbw...",
"wwwwwww..wbbbw.w.wwbbbbw.wbwbbbwwwwwbwwwb.wwbwbw..wwbbbw...wb.bw",
"..w.wb..w.wwww...wbbwwbb.bwbbbwbwbbbbwwbwwwbbbbbw.bwbbbb.bbbbbwb",
"..ww....w.www...wwwbbwbbwwbbbbwbwbwwbbwwbbwwwbbbb.bwwwbb.bbbbwwb",
".wwwww..b.wbbw..bwbwbbw.bbbwbwwwbbbbwwwwbbbbww.b.bbbbbw...wwwwww",
"...wbb..bb.wbbb.bbbbwwwwbbbbbwwwbbwbwwww.wbwbwwww.bbbb...wwwwwww",
"..wwww.bbbwwwwb.bbbwwbwwwbbbbwbbbbbbbww.bbbwbw.w.bbw.w...bbb.www",
"w.wbbbbw.wbbbbb.bbwbbwb..bwwbwbwbbbwbbbwwbbbbbbw..bbbbb....wwww.",
"..bb.b....bbbb.bwwwwwwbb.bwbwbbbbbbbbbbbbbbbbbwbb.wwwwwb..bwwwwb",
"..wwwb..b.wwbb..bbbbwbbbbbbwbbbbbbwbwwbb.bwbwbwww.bbbbw...wbbbbw",
"bbbbbw..bwwb.w..bbbwww..bbwwwww.bwwbwbw.bwbbwwbww.bbwbb..wwbbbbw",
"..bbbbb.w.bbbb.bwbbbbbbbwbbbbbwbwbwbwww.b.wwwww..bwwww...bbbbbbb",
"w.wbbbbw.wbbbbb.bbwbbwb..bwwbwbwbbbwbbbwwbbbbbbw..bbbbb....wwww.",
"..wbbbbw..bwwwwwwbwbwbwwbbbwwwbww.wbwbww.wbwbwww..bbwww..bbbbw..",
"..bbbb....bbbb..wwwwwbwbwwwwwwbbwwwbwwbbwwbwbwb.wbbbbbbw...bbbbw",
".bbbbbb.wwwwbw..wwwbwbw.wbwwwwb.wbbbbbbbwwwwwww.w.wbww....wbwwww",
".wwwwww...bbbb..b.bbwbbbwwbbwbb.bwbbwbb.bwwbbbb.wwwwbwb..bbbbbbb",

/* some trivial positions: */
"wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww..",
"wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwb.",
"wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwbw..",
"wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwbww.",
"wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwbw...",
"wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwbwbw....",
"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbwbw...",
"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbwb...",
"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbwb..",
"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbwb.",

"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbwbbbbbbbwbbbbbwb.",
"wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwbwwwwwwwbwwwwbbb.",
};
#ifdef WINDOWS_TIMING
   double start_time = get_real_time();
#endif
   for(i=0; i<112; i++){
      for(j=0; j<=90; j++) board[j] = DUMMY;
      wc=bc=emp=0;
      for(j=0; j<64; j++){
         x = j&7; y = (j>>3)&7; k = x+10+9*y;
         if(bds[i][j]=='w'){ board[k] = WHITE; wc++; }
         else if(bds[i][j]=='b'){ board[k] = BLACK; bc++; }
         else if(bds[i][j]=='.'){ board[k] = EMPTY; emp++; }
      }
      PrepareToSolve( board );
      for(tries=0; tries<MAXTRIES; tries++){
#if FULLSOLVE==2
         val = EndSolve( board, -64, 64, WHITE, emp, wc-bc, 1 );
#else
#if FULLSOLVE==1
         val = EndSolve( board, -1, 1, WHITE, emp, wc-bc, 1 );
#else
         val = EndSolve( board,  0, 1, WHITE, emp, wc-bc, 1 );
#endif
#endif
      }
      if (0) // BENCH HACK: disable most of the output
      printf("%3d (emp=%2d wc=%2d bc=%2d) %s\n",
         val,            emp,wc,bc,            bds[i]         );
   }

   printf(
   "MAXTRIES=%d. USE_PARITY=%d. FULLSOLVE=%d. WINNER_GETS_EMPTIES=%d. FASTEST_FIRST=%d.\n",
      MAXTRIES, USE_PARITY, FULLSOLVE, WINNER_GETS_EMPTIES, FASTEST_FIRST);
#ifdef WINDOWS_TIMING
   printf( "Time: %.2f seconds\n", get_real_time() - start_time );
#endif

   return EXIT_SUCCESS;
}


/******
end of file
********/




