# created by Joshua Shepherd

CC = mpic++ #nvcc
CPPFLAGS = -Wall -lm

#CFLAGS=-std=c99 -Wall -O0 -m32# -DVERBOSE# -Iinclude

#CFLAGS += #-Wno-unused-variable
#VPATH = src include
#CPPFLAGS=-std=c++11 -O3 -Wall
#LDFLAGS=
#automatically compile c++ files https://stackoverflow.com/a/2908351
SRC_DIR := .
OBJ_DIR := .
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

#INCLUDE=
OBJ = $(SRC:.cpp=.o)
TARGET = game_of_life
SUB_SCRIPT = sub.bash

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(LDFLAGS) $(CPPFLAGS) -o $(TARGET) $(OBJ_FILES)
	
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) $(CPPFLAGS) -c -o $@ $<
	
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
	rm -rf $(OBJ_FILES) $(TARGET) *.out
