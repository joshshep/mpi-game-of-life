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
#include <stdbool.h>

using namespace std;

#define NUM_TRIALS (100)

// inclusive, that is, it's possible to have a 99 (but not a 100)

#define BRD_HEIGHT     (16)
#define BRD_WIDTH      (16)
#define BRD_BUF_WIDTH  (BRD_WIDTH + 2)
#define BRD_PRINT_FREQ (1) // 1 -> print every cycle
#define NUM_GENS       (1)

enum LifeState {
  dead=false,
  alive=true
};

typedef struct cell {
  enum LifeState lifeState;
  char num_neighbors;
} Cell;

// modified from https://stackoverflow.com/a/6852396
// Assumes 0 <= max <= RAND_MAX
// Returns in the closed interval [0, max]
long random_at_most(long max) {
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
double avg_trials_permutation(int f(int,int,int), int rank, int p, int rand_int, int num_trials) {
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
int sync_print_once(int rank, const char* fmt, ...) {
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

int wrap_around_hori(bool** slice_buf, int w, int h) {
  // we need not round the buffer rows because they are wrapped in their own slice
  // (basically when they themselvse are not buffers)
  for (int row=1; row<h-1; ++row) {
    slice_buf[row][0]   = slice_buf[row][w-2];
    slice_buf[row][w-1] = slice_buf[row][1];
  }
  return 0;
}

int rand_populate_slice(bool** slice_buf, int w, int h) {
  for (int y=1; y<h-1; ++y) {
    for (int x=1; x<w-1; ++x) {
      slice_buf[y][x] = rand_bool();
    }
  }
  
  wrap_around_hori(slice_buf, w, h);
  
  return 0;
}

int send_row(bool* row, int w, int dest_rank) {
  return MPI_Send(row, w, MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);
}

int recv_row(bool* row, int w, int src_rank) {
  MPI_Status status;
  return MPI_Recv(row, w, MPI_BYTE, src_rank, 0, MPI_COMM_WORLD, &status);
}

int count_neighbors(bool** slice_buf, int x, int y) {
  int sum = 0;
  for (int i=x-1; i<=x+1; ++i) {
    for (int j=y-1; j<=y+1; ++j) {
      if (i!=x || j!=y)
        if (slice_buf[j][i])
          ++sum;
    }
  }
  return sum;
}

int run_life(bool** new_slice_buf, bool** slice_buf, int x, int y) {
  int num_neighbors = count_neighbors(slice_buf, x, y);
  if (slice_buf[y][x]) {
    // this is a live cell
    if (num_neighbors < 2 || num_neighbors > 3) {
      // dies by underpopulation
      // OR
      // dies by overpopulation
      new_slice_buf[y][x] = dead;
    } else {
      // this is only necessary since we've written it so new_slice_buf can be potentially unintialized
      new_slice_buf[y][x] = alive;
    }
  } else {
    // dead cell
    if (num_neighbors == 3) {
      // perfect conditions for life :)
      new_slice_buf[y][x] = alive;
    } else {
      new_slice_buf[y][x] = dead;
    }
  }
  return 0;
}

int print_slice(bool** slice_buf, int w, int h) {
  char sprites[2] = {'.','X'};
  for (int y=1; y<h-1; ++y) {
    for (int x=1; x<w-1; ++x) {
      printf("%c",sprites[slice_buf[y][x]]);
    }
    printf("\n");
  }
  return 0;
}

int print_board(int rank, int p, bool** slice_buf, int w, int h) {
  for (int r=0; r<p; ++r) {
    if (r == rank) {
      // its this rank's turn to print
      printf("r%2d\n", rank);
      print_slice(slice_buf, w, h);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
  return 0;
}

int cpy_brd(bool** dst, bool** src, int w, int h) {
  for (int y=0; y < h; ++y) {
    memcpy(dst[y], src[y], w*sizeof(bool));
  }
  return 0;
}

int simulate(int num_gens, int rank, int p, bool** slice_buf, int w, int h) {
  // allocate the memory for the temporary buffer
  bool** slice_buf_tmp = new bool*[h];
  for (int y=0; y<h; ++y) {
    slice_buf_tmp[y] = new bool[w];
  }
  
  for (int igen=0; igen < num_gens; ++igen) {
    wrap_around_hori(slice_buf, w, h);

    // send the row to the slice *above*
    send_row(slice_buf[1], w, (rank+p-1)%p);
    
    // send the row with the slice *below*
    send_row(slice_buf[h - 2], w, (rank+1)%p);
    
    // grab the row from the slice *above*
    recv_row(slice_buf[0], w, (rank+p-1)%p);
    
    // grab the row from the slice *below*
    recv_row(slice_buf[h - 1], w, (rank+1)%p);

    for (int y=1; y<h-1; ++y) {
      for (int x=1; x<w-1; ++x) {
        run_life(slice_buf_tmp, slice_buf, x, y);
      }
    }
    
    // to get the new slice in slice_buf simply switch the pointers
    bool** ptr_tmp = slice_buf_tmp;
    slice_buf_tmp = slice_buf;
    slice_buf = ptr_tmp;
    
    if (igen % BRD_PRINT_FREQ == 0) {
      const char* plural[2] = {"", "s"};
      sync_print_once(rank, "Board after %3d generation%s\n",igen+1, plural[igen!=1]);
      print_board(rank, p, slice_buf, w, h);
    }
  }
  
  if (num_gens % 2 == 1) {
    // uneven number of swaps in the simulation loop
    bool** ptr_tmp = slice_buf_tmp;
    slice_buf_tmp = slice_buf;
    slice_buf = ptr_tmp;
  }

  // deallocate the temporary buffer
  for (int y=0; y<h; ++y) {
    delete[] slice_buf_tmp[y];
  }
  delete[] slice_buf_tmp;
  return 0;
}

class Slice {
public:
  Slice(int w, int h);
  ~Slice();
  int printSlice();
  int wrapAroundHori();

  bool** buf;
  int width, height;
  int buf_width, buf_height;
  int buf_size;
};


Slice::Slice(int w, int h) {
  this->width = w;
  this->height = h;

  this->buf_width = w+2;
  this->buf_height = h+2;
  this->buf_size = buf_width * buf_height;
  // allocate the memory for storing the slice
  // allocate contiguously to make sending/receiving simpler

  bool* contiguousArr = new bool[buf_width*buf_height];
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
  for (int y=1; y<buf_width-1; ++y) {
    buf[y][0]           = buf[y][buf_width-2];
    buf[y][buf_width-1] = buf[y][1];
  }
  return 0;
}

class GameOfLife {
public:
  //GameOfLife(int rank, int p, int w, int h): rank(rank), p(p), w(w), h(h) {}
  GameOfLife(int rank, int p, int brd_w, int brd_h);

  int print_brd();
private:

  int wrap_around_hori();

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


  int slice_height = brd_h / p;
  if (rank < (brd_h % p)) {
    this->slice_row_start = rank*slice_height + rank;
    ++slice_height;
  } else {
    this->slice_row_start = rank*slice_height + (brd_h % p);
  }

  this->slice  = new Slice(brd_w, slice_height);
  this->slice2 = new Slice(brd_w, slice_height);
}

int GameOfLife::print_brd() {
  if (rank > 0) {
    // send the slice to rank 0
    int dest_rank = 0;
    MPI_Send(slice->buf, slice->buf_size, MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);
  } else {
    // receive all of the slices and print them to stdout

    //first we need to print the first slice (rank0)
    MPI_Status status;
    for (int src_rank=1; src_rank < p; ++src_rank) {
      MPI_Recv(slice2->buf, slice2->buf_size, MPI_BYTE, src_rank, 0, MPI_COMM_WORLD, &status);

    }
  }
  return 0;
}

int main(int argc, char** argv, char** envp) {
  assert(sizeof(bool) == 1);
  
  //struct timeval t1, t2;

  int rank, p;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &p);


  // seed the random function differently for each processor
  srand(time(NULL) + rank);
  
  /*
  get the heights of the slices (of which there are rank) containing all rows.
  For example: 5 processors, 102 rows
  i0: 21
  i1: 21
  i2: 20
  i3: 20
  i4: 20
  */
  
  assert(p);
  
  int slice_row_start;
  int slice_height = BRD_HEIGHT / p;
  if (rank < (BRD_HEIGHT % p)) {
    slice_row_start = rank*slice_height + rank;
    ++slice_height;
  } else {
    slice_row_start = rank*slice_height + (BRD_HEIGHT % p);
  }
  
  assert(slice_height); // ? we have more processors than rows
  
  printf("r%2d: slice_height: %3d ; slice_row_start: %3d\n", rank, slice_height, slice_row_start);
  
  int slice_buf_height = slice_height + 2;
  /*
  Note the disctinction between slice_height and slice_buf_height
  We make the slice wider (by two cells) and taller (by two rows) to make the edge cases easier
  */
  
  // allocate the memory for storing the slice
  // allocate contiguously to make sending/receiving simpler
  bool* contiguousArr = new bool[slice_buf_height*BRD_BUF_WIDTH];
  bool** slice_buf     = new bool*[slice_buf_height];
  for (int row=0; row<slice_buf_height; ++row) {
    slice_buf[row] = contiguousArr + row*BRD_BUF_WIDTH;
  }
  
  rand_populate_slice(slice_buf, BRD_BUF_WIDTH, slice_buf_height);
  
  sync_print_once(rank, "Board after random initialization\n");
  print_board(rank, p, slice_buf, BRD_BUF_WIDTH, slice_buf_height);
  /*
  By convention:
  
  -----------------------------------
  | | | | | | | | | | | | | | | | | |
  -----------------------------------       rank0
  | | | | | | | | | | | | | | | | | |
  -----------------------------------
  
  -----------------------------------
  | | | | | | | | | | | | | | | | | |
  -----------------------------------       rank1
  | | | | | | | | | | | | | | | | | |
  -----------------------------------
  
  -----------------------------------
  | | | | | | | | | | | | | | | | | |
  -----------------------------------       rank2
  | | | | | | | | | | | | | | | | | |
  -----------------------------------
  */
  
  //cpy_brd(slice_buf_tmp, slice_buf, BRD_BUF_WIDTH, slice_buf_height);
  simulate(NUM_GENS, rank, p, slice_buf,  BRD_BUF_WIDTH, slice_buf_height);
  
  
  
  //deallocate the slice's memory
  delete[] slice_buf[0];
  delete[] slice_buf;
  
  MPI_Finalize();
  return 0;
}
































