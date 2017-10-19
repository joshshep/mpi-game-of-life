#ifndef SLICE_H
#define SLICE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <mpi.h>

using namespace std;

enum LifeState {
  dead = false,
  alive= true
};

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

  //DEBUG
  int printBuf();

private:
  char cell_sprites[2];
  bool randBool();
};

#endif