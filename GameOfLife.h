#ifndef GAME_OF_LIFE_H
#define GAME_OF_LIFE_H

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <mpi.h>

#include "Slice.h"

using namespace std;

class GameOfLife {
public:
  //GameOfLife(int rank, int p, int w, int h): rank(rank), p(p), w(w), h(h) {}
  GameOfLife(int rank, int p, int brd_w, int brd_h);
  ~GameOfLife();
  int printBoard();
  int printBoardBuf();
  int shareRows();
  int runLife(Slice* dest_slice, Slice* src_slice);
  int runLife(Slice* dest_slice, Slice* src_slice, int x, int y);
  int simulate(int num_gens, int print_freq);
  int clear();
  int drawGlider();
  int randPopulate();
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

int syncPrintOnce(int rank, const char* fmt, ...);

#endif