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

  slice->randPopulate();

  // fill in the edges of the slice buf
  slice->wrapAroundHori();
  shareRows();
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

int GameOfLife::simulate(int num_gens, int print_freq) {
  for (int igen=0; igen < num_gens; ++igen) {
    runLife(slice2, slice);
    
    // to get the new slice in slice_buf simply switch the pointers
    swap(slice, slice2);

    // fill in the edges of the slice buf
    slice->wrapAroundHori();
    shareRows();
    
    if (igen % print_freq == 0) {
      const char* plural[2] = {"", "s"};
      syncPrintOnce(rank, "\nBoard after %d generation%s\n", igen+1, plural[(igen+1)!=1]);
      syncPrintOnce(rank, "------------------------------\n");
      printBoard();

      // TODO: does this need to be here?
      // My concern is that rank 1 will loop to sendRowTo() and then there 
      // would be 2 messages pending for rank 0 to receive from rank 1
      //MPI_Barrier(MPI_COMM_WORLD);
    }
  }


  if ((num_gens - 1) % print_freq != 0) {
    const char* plural[2] = {"", "s"};
    syncPrintOnce(rank, "\nBoard after %d generation%s\n", num_gens - 1, plural[(num_gens - 1)!=1]);
    syncPrintOnce(rank, "------------------------------\n");
    printBoard();

    // TODO: does this need to be here?
    // My concern is that rank 1 will loop to sendRowTo() and then there 
    // would be 2 messages pending for rank 0 to receive from rank 1
    //MPI_Barrier(MPI_COMM_WORLD);
  }
  
  // if (num_gens % 2 == 1) {
  //   // uneven number of swaps in the simulation loop
  //   // swap back to get the most recent slice in "slice"
  //   swap(slice, slice2);
  // }

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
