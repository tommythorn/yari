/* program to solve 2x2 go with area rules and positional superko */
/* note that suicide is not possible in this case */

#include <stdio.h>

#define NSHOW 4
#define NMOVES 4
int h[256]; /* bitmap of positions in game history */
int count[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
int nodes[99];

void show(int n, int black, int white, int alpha, int beta, int passed)
	/* print 2-line ascii representation of position */
{
  int i;
  printf("%d (%d,%d)%s\n", n, alpha, beta, passed?" pass":"");
  for (i=0; i<4; i++) {
    printf(" %c", ".XO#"[(black>>i&1)+2*(white>>i&1)]);
    if (i&1==1)
      putchar('\n');
  }
}

int visit(int black, int white)
{
  h[black +16* white] = 1;
}

int unvisit(int black, int white)
{
  h[black +16* white] = 0;
}

int visited(int black, int white)
{
  return h[black +16* white];
}

int score(int black, int white)
{
  if (black==0)
    return white ? -4 : 0;
  if (white==0)
    return 4;
  else return count[black]-count[white];
}

int xhasmove(int black, int white, int move, int *newblack, int *newwhite)
{
  move = 1<<move;
  if (black&move || white&move || count[black]==3 || white==6 || white==9)
    return 0;
  *newblack = black|move;
  *newwhite = *newblack+white==15 || *newblack==6 || *newblack==9 ? 0 : white;
  return !h[*newblack +16* *newwhite];
}

int ohasmove(int black, int white, int move, int *newblack, int *newwhite)
{
  move = 1<<move;
  if (black&move || white&move || count[white]==3 || black==6 || black==9)
    return 0;
  *newwhite = white|move;
  *newblack = *newwhite+black==15 || *newwhite==6 || *newwhite==9 ? 0 : black;
  return !h[*newblack +16* *newwhite];
}

int xab(int n, int black, int white, int alpha, int beta, int passed)
{
  int i,s,newblack,newwhite;

  nodes[n]++;
  if (n<NSHOW)
    show(n,black,white,alpha,beta,passed);

  s = passed ? score(black, white) : oab(n+1,black,white,alpha,beta,1);
  if (s > alpha && (alpha=s) >= beta)
    return alpha;
  for (i=0; i<NMOVES; i++) {
    if (xhasmove(black,white,i,&newblack,&newwhite)) {
      visit(newblack,newwhite);
      s = oab(n+1, newblack, newwhite, alpha, beta, 0);
      unvisit(newblack,newwhite);
      if (s > alpha && (alpha=s) >= beta)
        return alpha;
    }
  }
  return alpha;
}

int oab(int n, int black, int white, int alpha, int beta, int passed)
{
  int i,s,newblack,newwhite;

  nodes[n]++;
  if (n<NSHOW)
    show(n,black,white,alpha,beta,passed);

  s = passed ? score(black, white) : xab(n+1,black,white,alpha,beta,1);
  if (s < beta && (beta=s) <= alpha)
    return beta;
  for (i=0; i<NMOVES; i++) {
    if (ohasmove(black,white,i,&newblack,&newwhite)) {
      visit(newblack,newwhite);
      s = xab(n+1, newblack, newwhite, alpha, beta, 0);
      unvisit(newblack,newwhite);
      if (s < beta && (beta=s) <= alpha)
        return beta;
    }
  }
  return beta;
}

main()
{
  int i,c,s;
  c = xab(0, 0, 0, -4, 4, 0);
  for (i=s=0; nodes[i]; i++) {
    s += nodes[i];
    printf("%d: %d\n", i, nodes[i]);
  }
  printf("total: %d\nx wins by %d\n", s, c);
}
