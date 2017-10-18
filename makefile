# created by Joshua Shepherd

CC=mpicc #nvcc
CFLAGS=-Wall -std=c99 -lm

#CFLAGS=-std=c99 -Wall -O0 -m32# -DVERBOSE# -Iinclude

#CFLAGS += #-Wno-unused-variable
#VPATH = src include
#CPPFLAGS=-std=c++11 -O3 -Wall
#LDFLAGS=
SRC=game_of_life.c
#INCLUDE=
OBJ = $(SRC:.c=.o)
TARGET = game_of_life
SUB_SCRIPT = sub.bash

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)
	
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
run: all
	sbatch -N2 $(SUB_SCRIPT)

watch_run: all
	sbatch -N2 $(SUB_SCRIPT)
	./monitor.bash

watch_run_all: all
	sbatch -N1 sub1.bash
	sbatch -N2 sub2.bash
	sbatch -N4 sub4.bash
	sbatch -N8 sub8.bash
	sbatch -N8 sub16.bash
	sbatch -N8 sub32.bash
	sbatch -N8 sub64.bash
	./monitor.bash
#concise:

#disk:

clean:
	rm -rf $(OBJ) $(TARGET) *.out
