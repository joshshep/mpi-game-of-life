#include "GameOfLife.h"

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
  
  clear();
  
}

GameOfLife::~GameOfLife() {
  delete slice;
  delete slice2;
}

/**
 * Send all slices to rank 0 to print to stdout
 * 
*/
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


/**
 * Send all slice buffers (including duplicated columns and rows) to rank 0 to print to stdout
*/
int GameOfLife::printBoardBuf() {
  if (rank > 0) {
    // send the slice to rank 0
    int dest_rank = 0;
    MPI_Send(slice->buf[0], slice->buf_size, MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);
  } else {
    // receive all of the slices and print them to stdout
    //first we need to print the first slice (rank0)
    printf("r0:\n");
    slice->printBuf();

    MPI_Status status;
    for (int src_rank=1; src_rank < p; ++src_rank) {
      MPI_Recv(slice2->buf[0], slice2->buf_size, MPI_BYTE, src_rank, 0, MPI_COMM_WORLD, &status);
      printf("r%d:\n", src_rank);
      slice2->printBuf();
    }
  }
  //MPI_Barrier(MPI_COMM_WORLD);
  return 0;
}

/**
 * Send/recv the edge rows on slices to/from other slices
 * 
*/
int GameOfLife::shareRows() {
  // send the row to the slice *above*
  slice->sendRowTo(1, (rank+p-1)%p, sendToAbove);
  
  // send the row with the slice *below*
  slice->sendRowTo(slice->buf_height - 2, (rank+1)%p, sendToBelow);

  // grab the row from the slice *above*
  slice->recvRowFrom(0, (rank+p-1)%p, recvFromAbove);
  
  // grab the row from the slice *below*
  slice->recvRowFrom(slice->buf_height - 1, (rank+1)%p, recvFromBelow);
  
  return 0;
}



/**
 * Main processing loop.
 * Simulates num_gens generations with print frequency print_freq  (e.g. 1 -> print every generation)
*/
struct Timings GameOfLife::simulate(int num_gens, int print_freq) {
  struct Timings timings = {0};

  struct timeval t1_running, t2_running;
  struct timeval t1_comm, t2_comm;

  gettimeofday(&t1_running, NULL);
  
  for (int igen=0; igen < num_gens; ++igen) {
  
    //run life (main computation)
    runLife(slice2, slice);

    // to get the new slice in slice_buf simply switch the pointers
    swap(slice, slice2);

    // fill in the edges of the slice buf
    slice->wrapAroundHori();
    
    

    //communcation
    gettimeofday(&t1_comm, NULL);
    shareRows();
    gettimeofday(&t2_comm, NULL);
    timings.comm += (t2_comm.tv_sec-t1_comm.tv_sec)*1e6 + (t2_comm.tv_usec-t1_comm.tv_usec);

    if (print_freq > 0 && igen % print_freq == 0) {
      const char* plural[2] = {"", "s"};
      syncPrintOnce(rank, "\nBoard after %d generation%s\n", igen+1, plural[(igen+1)!=1]);
      syncPrintOnce(rank, "------------------------------\n");
      printBoard();

      //MPI_Barrier(MPI_COMM_WORLD);
    }
    
    
  }

  // print one last time if we haven't printed the last generation
  if (print_freq > 0 && (num_gens - 1) % print_freq != 0) {
    const char* plural[2] = {"", "s"};
    syncPrintOnce(rank, "\nBoard after %d generation%s\n", num_gens - 1, plural[(num_gens - 1)!=1]);
    syncPrintOnce(rank, "------------------------------\n");
    printBoard();
    //MPI_Barrier(MPI_COMM_WORLD);
  }

  gettimeofday(&t2_running, NULL);
  timings.running += (t2_running.tv_sec-t1_running.tv_sec)*1e6 + (t2_running.tv_usec-t1_running.tv_usec);
  
  return timings;
}

int GameOfLife::avgTimeSimulate(int num_gens, int print_freq) {
  struct Timings timings = simulate(num_gens, print_freq);
  if (rank > 0) {
    // send timings
    int dest_rank = 0;
    MPI_Send(&timings, sizeof(struct Timings), MPI_BYTE, dest_rank, 0, MPI_COMM_WORLD);
    return 0; 
  }
  //printf("t%2d: running: %12lu ; comm: %12lu\n", 0, timings.running, timings.comm);
  MPI_Status status;
  struct Timings timings_runner = {0};
  // rank 0
  for (int src_rank=1; src_rank<p; ++src_rank) {
    MPI_Recv(&timings_runner, sizeof(struct Timings), MPI_BYTE, src_rank, 0, MPI_COMM_WORLD, &status);
    //printf("t%2d: running: %12lu ; comm: %12lu\n", src_rank, timings_runner.running, timings_runner.comm);
    
    timings.running += timings_runner.running;
    timings.comm += timings_runner.comm;
  }
  double avg_running    = (double) timings.running / (p * num_gens);
  double avg_comm    = (double) timings.comm / (p * num_gens);
  double avg_comp = (double) (timings.running - timings.comm) / (p * num_gens);
  
  printf("AVG\n");
  printf("running:       %lf\n", avg_running);
  printf("communication: %lf\n", avg_comm);
  printf("computation:   %lf\n", avg_comp);
  
  return 0;
}

/**
 * Run life on src_slice and put the result in dest_slice
*/
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



/**
 * TEST
 * Draw a glider in rank 0's slice
 * 
*/
int GameOfLife::drawGlider() {
  if (rank != 0) {
    if (slice->height < 3 || slice->width < 3) {
      printf("Couldn't print glider (slice too small)\n");
    } else {
      slice->drawGlider();
    }
  }

  // fill in the edges of the slice buf
  slice->wrapAroundHori();
  shareRows();
  
  return 0;
}

int GameOfLife::randPopulate() {
  slice->randPopulate();
  
  // fill in the edges of the slice buf
  slice->wrapAroundHori();
  shareRows();
  return 0;
}

int GameOfLife::clear() {
  slice->clear();
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
