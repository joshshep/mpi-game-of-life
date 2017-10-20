/*	Cpt S 483, Introduction to Parallel Computing
 *	School of Electrical Engineering and Computer Science
 *
 *  Joshua Shepherd
 *  2017-10-03
 * */


#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <algorithm>

#include "GameOfLife.h"

using namespace std;

#define NUM_TRIALS (100)

// inclusive, that is, it's possible to have a 99 (but not a 100)

#define BRD_HEIGHT     (16)
#define BRD_WIDTH      (16)
#define BRD_BUF_WIDTH  (BRD_WIDTH + 2)
#define BRD_PRINT_FREQ (0) // 1 -> print every cycle
#define NUM_GENS       (1000)


typedef struct cell {
  enum LifeState lifeState;
  char num_neighbors;
} Cell;

// modified from https://stackoverflow.com/a/6852396
// Assumes 0 <= max <= RAND_MAX
// Returns in the closed interval [0, max]
long randomAtMost(long max) {
  unsigned long
    // max <= RAND_MAX < ULONG_MAX, so this is okay.
    num_bins = (unsigned long) max + 1,
    num_rand = (unsigned long) RAND_MAX + 1,
    bin_size = num_rand / num_bins,
    defect   = num_rand % num_bins;

  long x;
  do {
   x = rand();
  }
  // This is carefully written not to overflow
  while (num_rand - defect <= (unsigned long)x);

  // Truncated division is intentional
  return x/bin_size;
}



int main(int argc, char** argv, char** envp) {
  assert(sizeof(char) == 1);
  
  //struct timeval t1, t2;

  int rank, p;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &p);
  syncPrintOnce(rank, "################################################\n");
  syncPrintOnce(rank, "################################################\n");
  syncPrintOnce(rank, "################################################\n");
  syncPrintOnce(rank, "p=%d, %d generations\n", p, NUM_GENS);
  
  /*
  get the heights of the slices (of which there are rank) containing all rows.
  For example: 5 processors, 102 rows
  i0: 21
  i1: 21
  i2: 20
  i3: 20
  i4: 20
  */
  //syncPrintOnce(rank, "Board dimensions x,y: %d, %d\n", BRD_WIDTH, BRD_HEIGHT);
  
  // GameOfLife game(rank, p, BRD_WIDTH, BRD_HEIGHT);
  // game.randPopulate();
  // syncPrintOnce(rank, "Randomly initialized board\n");
  // syncPrintOnce(rank, "------------------------------\n");
  // game.printBoard();

  // game.simulate(NUM_GENS, BRD_PRINT_FREQ);
  int starting_board_size_power = 2;
  int last_board_size_power = 10;
  for (int board_size_power = starting_board_size_power; 
    board_size_power <= last_board_size_power; 
    ++board_size_power)
  {
    int board_size = 1 << board_size_power;
    syncPrintOnce(rank, "---------------------------------\n", BRD_WIDTH, BRD_HEIGHT);
    syncPrintOnce(rank, "Board dimensions x,y: %d, %d\n", board_size, board_size);
    if (board_size < p) {
      syncPrintOnce(rank, "More processors than rows ... skipping (%d > %d)\n", p, board_size);
      continue;
    }
    GameOfLife game(rank, p, board_size, board_size);

    // TEST
    //game.drawGlider();
    
    game.randPopulate();

    game.avgTimeSimulate(NUM_GENS, BRD_PRINT_FREQ);
  }

  /*
  GameOfLife game(rank, p, BRD_WIDTH, BRD_HEIGHT);
  game.drawGlider();
  syncPrintOnce(rank, "Board with glider\n");
  syncPrintOnce(rank, "------------------------------\n");
  game.printBoard();

  game.avgTimeSimulate(NUM_GENS, BRD_PRINT_FREQ);
  */
  MPI_Finalize();
  return 0;
}
































