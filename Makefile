PROJECT_NAME = grosfs
CXX = g++ 
ROOT_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
INC = $(ROOT_DIR)/include
SRC = $(ROOT_DIR)/src
CFLAGS = -Wall -m32 -g -I$(INC) -I$(SRC)

HEADERS = disk.hpp grosfs.hpp
FILES = main.cpp disk.cpp
EXECUTABLES = $(PROJECT_NAME)

all: $(EXECUTABLES)

SOURCES = $(FILES:%.cpp=$(SRC)/%.cpp)

grosfs: $(SOURCES) 
	$(CXX) $(CFLAGS) -o $(PROJECT_NAME) $(SOURCES) 

run: $(PROJECT_NAME)
	./$(PROJECT_NAME)

clean:
	/bin/rm -rf $(wildcard *.dSYM)
	/bin/rm -f $(EXECUTABLES)
