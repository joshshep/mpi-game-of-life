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

using namespace std;

#define NUM_TRIALS (100)

// inclusive, that is, it's possible to have a 99 (but not a 100)

#define BRD_HEIGHT     (4)
#define BRD_WIDTH      (4)
#define BRD_BUF_WIDTH  (BRD_WIDTH + 2)
#define BRD_PRINT_FREQ (1) // 1 -> print every cycle
#define NUM_GENS       (1)

enum LifeState {
  dead = false,
  alive= true
};

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

/*
Sync the processors (with barriers) and only print if rank==0
Otherwise acts as printf
*/
int syncPrintOnce(int rank, const char* fmt, ...) {
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank == 0) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
  MPI_Barrier(MPI_COMM_WORLD);
  return 0;
}

bool rand_bool() {
  return rand() & 1;
}

class Slice {
public:
  Slice(int w, int h);
  ~Slice();
  int print();
  int wrapAroundHori();
  int randPopulate();

  int sendTo(int dest_rank);
  int sendRowTo(int row, int dest_rank);
  int recvFrom(int src_rank);
  int recvRowFrom(int row, int src_rank);
  
  bool** buf;
  int width, height;
  int buf_width, buf_height;
  int buf_size;
private:
  char cell_sprites[2];
  bool randBool();
};


Slice::Slice(int w, int h) {
  this->width = w;
  this->height = h;

  this->buf_width = w+2;
  this->buf_height = h+2;
  this->buf_size = buf_width * buf_height;

  this->cell_sprites[dead]  = '.';
  this->cell_sprites[alive] = 'X';

  // allocate the memory for storing the slice
  // allocate contiguously to make sending/receiving simpler

  bool* contiguousArr = new bool [buf_width*buf_height];
  this->buf           = new bool*[buf_height];
  for (int y=0; y<buf_height; ++y) {
    this->buf[y] = contiguousArr + y*buf_width;
  }
}

Slice::~Slice() {
  delete[] buf[0];
  delete[] buf;
}

int Slice::wrapAroundHori() {
  // we need not round the buffer rows because they are wrapped in their own slice
  // (basically when they themselvse are not buffers)
  for (int y=1; y<buf_height-1; ++y) {
    buf[y][0]           = buf[y][buf_width-2];
    buf[y][buf_width-1] = buf[y][1];
  }
  return 0;
}

int Slice::print() {
  for (int y=1; y<buf_height-1; ++y) {
    for (int x=1; x<buf_width-1; ++x) {
      printf("%c", cell_sprites[buf[y][x]]);
    }
    printf("\n");
  }
  return 0;
}

int Slice::randPopulate() {
  for (int y=1; y<buf_height-1; ++y) {
    for (int x=1; x<buf_width-1; ++x) {
      buf[y][x] = randBool();
    }
  }
  
  //wrap_around_hori(buf, w, h);
  return 0;
}

/**
 * Note: this sends the entire buffer containing the slice (h+2, w+2)
 * 
 * **/
int Slice::sendTo(int dest_rank) {
  MPI_Send(buf[0], buf_size, MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);
  return 0;
}

/**
 * Note: this receives the entire buffer containing the slice (h+2, w+2)
 * 
 * **/
int Slice::recvFrom(int src_rank) {
  MPI_Status status;
  MPI_Recv(buf[0], buf_size, MPI_BYTE, src_rank, 0, MPI_COMM_WORLD, &status);
  return 0;
}

/**
 * Note: row==0 means this->buf[0]
 * 
 * **/
int Slice::sendRowTo(int row, int dest_rank) {
  MPI_Send(buf[row], buf_width, MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);
  return 0;
}

/**
 * Note: row==0 means this->buf[0]
 * 
 * **/
int Slice::recvRowFrom(int row, int src_rank) {
  MPI_Status status;
  MPI_Recv(buf[row], buf_width, MPI_BYTE, src_rank, 0, MPI_COMM_WORLD, &status);
  return 0;
}

bool Slice::randBool() {
  return rand() & 1;
}

class GameOfLife {
public:
  //GameOfLife(int rank, int p, int w, int h): rank(rank), p(p), w(w), h(h) {}
  GameOfLife(int rank, int p, int brd_w, int brd_h);
  ~GameOfLife();
  int printBoard();
  int runLife(Slice* dest_slice, Slice* src_slice);
  int runLife(Slice* dest_slice, Slice* src_slice, int x, int y);
  int simulate(int num_gens);
private:

  int countNeighbors(Slice* s, int x, int y);

  // the slice buffer is 2 rows taller and 2 columns wider than the slice
  Slice* slice;
  Slice* slice2;

  int rank, p;
  int brd_w, brd_h;
  int slice_w, slice_h;
  int slice_buf_w, slice_buf_h;

  int slice_row_start;

};

GameOfLife::GameOfLife(int rank, int p, int brd_w, int brd_h) {
  this->rank  = rank;
  this->p     = p;
  this->brd_w = brd_w;
  this->brd_h = brd_h;

  // seed the random function differently for each processor
  srand(time(NULL) + rank);

  // determine this slice's height
  int slice_height = brd_h / p;
  if (rank < (brd_h % p)) {
    this->slice_row_start = rank*slice_height + rank;
    ++slice_height;
  } else {
    this->slice_row_start = rank*slice_height + (brd_h % p);
  }

  assert(slice_height);

  this->slice  = new Slice(brd_w, slice_height);
  this->slice2 = new Slice(brd_w, slice_height);

  slice->randPopulate();
}

GameOfLife::~GameOfLife() {
  delete slice;
  delete slice2;
}

int GameOfLife::printBoard() {
  if (rank > 0) {
    // send the slice to rank 0
    int dest_rank = 0;
    MPI_Send(slice->buf[0], slice->buf_size, MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);
  } else {
    // receive all of the slices and print them to stdout
    //first we need to print the first slice (rank0)
    printf("r0:\n");
    slice->print();

    MPI_Status status;
    for (int src_rank=1; src_rank < p; ++src_rank) {
      MPI_Recv(slice2->buf[0], slice2->buf_size, MPI_BYTE, src_rank, 0, MPI_COMM_WORLD, &status);
      printf("r%d:\n", src_rank);
      slice2->print();
    }
  }
  //MPI_Barrier(MPI_COMM_WORLD);
  return 0;
}

int GameOfLife::simulate(int num_gens) {
  for (int igen=0; igen < num_gens; ++igen) {
    slice->wrapAroundHori();

    // send the row to the slice *above*
    slice->sendRowTo(0, (rank+p-1)%p);

    // send the row with the slice *below*
    slice->sendRowTo(slice->buf_height - 1, (rank+1)%p);

    // grab the row from the slice *above*
    slice->recvRowFrom(0, (rank+p-1)%p);
    
    // grab the row from the slice *below*
    slice->recvRowFrom(slice->buf_height - 1, (rank+1)%p);
    
    for (int y=1; y < slice->buf_height-1; ++y) {
      for (int x=1; x < slice->buf_width-1; ++x) {
        runLife(slice2, slice, x, y);
      }
    }
    
    // to get the new slice in slice_buf simply switch the pointers
    swap(slice, slice2);
    
    if (igen % BRD_PRINT_FREQ == 0) {
      const char* plural[2] = {"", "s"};
      syncPrintOnce(rank, "Board after %3d generation%s\n",igen+1, plural[(igen+1)!=1]);
      printBoard();

      // TODO: does this need to be here?
      // My concern is that rank 1 will loop to sendRowTo() and then there 
      // would be 2 messages pending for rank 0 to receive from rank 1
      //MPI_Barrier(MPI_COMM_WORLD);
    }
  }
  
  if (num_gens % 2 == 1) {
    // uneven number of swaps in the simulation loop
    // swap back to get the most recent slice in "slice"
    swap(slice, slice2);
  }

  return 0;
}

int GameOfLife::runLife(Slice* dest_slice, Slice* src_slice) {
  for (int y=1; y < src_slice->buf_height - 1; ++y) {
    for (int x=1; x < src_slice->buf_width - 1; ++x) {
      runLife(dest_slice, src_slice, x, y);
    }
  }
  return 0;
}

int GameOfLife::runLife(Slice* dest_slice, Slice* src_slice, int x, int y) {
  int num_neighbors = countNeighbors(src_slice, x, y);
  if (src_slice->buf[y][x] == alive) {
    // this is a live cell
    if (num_neighbors < 2 || num_neighbors > 3) {
      // dies by underpopulation
      // OR
      // dies by overpopulation
      dest_slice->buf[y][x] = dead;
    } else {
      // this is only necessary since we've written it so new_slice_buf can be potentially unintialized
      dest_slice->buf[y][x] = alive;
    }
  } else {
    // dead cell
    if (num_neighbors == 3) {
      // perfect conditions for life :)
      dest_slice->buf[y][x] = alive;
    } else {
      dest_slice->buf[y][x] = dead;
    }
  }
  return 0;
}

int GameOfLife::countNeighbors(Slice* s, int x, int y) {
  int sum = 0;
  for (int i=x-1; i<=x+1; ++i) {
    for (int j=y-1; j<=y+1; ++j) {
      if (s->buf[j][i] == alive)
        ++sum;
    }
  }
  // return the sum of all 9 cells (minus 1 if the middle is alive)
  return sum - (s->buf[y][x] == alive);
}

int main(int argc, char** argv, char** envp) {
  assert(sizeof(bool) == 1);
  
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

  syncPrintOnce(rank, "Randomly initialized board\n");
  syncPrintOnce(rank, "------------------------------\n");
  game.printBoard();
  game.simulate(NUM_GENS);
  
  MPI_Finalize();
  return 0;
}
































