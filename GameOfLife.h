#ifndef GAME_OF_LIFE_H
#define GAME_OF_LIFE_H

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <mpi.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>


#include "Slice.h"

using namespace std;

struct Timings {
  uint64_t running;
  uint64_t comm;
};

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
  struct Timings simulate(int num_gens, int print_freq);
  int avgTimeSimulate(int num_gens, int print_freq);
  int clear();
  int drawGlider();
  int randPopulate();

  // function aliases as specified in project outline
  // these names don't really follow the style guide so they are simply wrappers
  int Simulate();
  int DisplayGoL();
  int GenerateGoL();
private:
  int countNeighbors(Slice* s, int x, int y);

  // the slice buffer is 2 rows taller and 2 columns wider than the slice itself
  Slice* slice;
  Slice* slice2;

  int rank, p;
  int brd_w, brd_h;
  int slice_w, slice_h;
  int slice_buf_w, slice_buf_h;

  int slice_row_start;

  // uint64_t time_run = 0;
  // uint64_t time_comm = 0;
  // uint

};

int syncPrintOnce(int rank, const char* fmt, ...);

#endif