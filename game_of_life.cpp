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

#define BRD_HEIGHT     (4)
#define BRD_WIDTH      (4)
#define BRD_BUF_WIDTH  (BRD_WIDTH + 2)
#define BRD_PRINT_FREQ (1) // 1 -> print every cycle
#define NUM_GENS       (1)


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
/*

Accepts a function to time and its parameters and averages over num_trials trials

*/
double avgTrialsPermutation(int f(int,int,int), int rank, int p, int rand_int, int num_trials) {
  uint64_t usecs = 0;
  struct timeval t1, t2;
  int expected_sum = -1; // correct sums must always be non-negative
  
  for (int i=0; i< num_trials; ++i) {
    MPI_Barrier(MPI_COMM_WORLD);
    gettimeofday(&t1,NULL);
    int sum = f(rank, p, rand_int);
    
    //make sure all procs are finished before declaring "Done"
    MPI_Barrier(MPI_COMM_WORLD);
    gettimeofday(&t2,NULL);
    uint64_t usec = (t2.tv_sec-t1.tv_sec)*1e6 + (t2.tv_usec-t1.tv_usec);
    usecs += usec;
    
    // sanity check
    assert(sum == expected_sum || expected_sum == -1);
    expected_sum = sum;
  }
  return (double) usecs / num_trials;
}



int main(int argc, char** argv, char** envp) {
  assert(sizeof(char) == 1);
  
  //struct timeval t1, t2;

  int rank, p;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &p);
  
  /*
  get the heights of the slices (of which there are rank) containing all rows.
  For example: 5 processors, 102 rows
  i0: 21
  i1: 21
  i2: 20
  i3: 20
  i4: 20
  */
  
  GameOfLife game(rank, p, BRD_WIDTH, BRD_HEIGHT);

  syncPrintOnce(rank, "Randomly initialized board buf\n");
  syncPrintOnce(rank, "------------------------------\n");
  game.printBoard();

  game.simulate(NUM_GENS, BRD_PRINT_FREQ);
  
  MPI_Finalize();
  return 0;
}
































