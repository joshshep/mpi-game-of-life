#include "Slice.h"

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

int Slice::printBuf() {
  for (int y=0; y<buf_height; ++y) {
    for (int x=0; x<buf_width; ++x) {
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