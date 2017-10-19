#ifndef SLICE_H
#define SLICE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <mpi.h>

using namespace std;

enum LifeState {
  dead = 0,
  alive= 1
};

enum DirectionTag {
  sendToAbove = 1,
  recvFromBelow = 1,
  sendToBelow = 2,
  recvFromAbove = 2
};

class Slice {
public:
  Slice(int w, int h);
  ~Slice();
  int print();
  int wrapAroundHori();
  int randPopulate();

  int sendTo(int dest_rank, enum DirectionTag tag);
  int sendRowTo(int row, int dest_rank, enum DirectionTag tag);
  int recvFrom(int src_rank, enum DirectionTag tag);
  int recvRowFrom(int row, int src_rank, enum DirectionTag tag);
  
  char** buf;
  int width, height;
  int buf_width, buf_height;
  int buf_size;

  //DEBUG
  int printBuf();

private:
  char cell_sprites[2];
  char randBool();
};

#endif