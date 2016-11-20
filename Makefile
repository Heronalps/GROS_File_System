PROJECT_NAME = grosfs
CXX = g++
ROOT_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
INC = $(ROOT_DIR)/include
SRC = $(ROOT_DIR)/src
CFLAGS = -Wall -g -isystem $(INC) -I$(SRC)

HEADERS = disk.hpp grosfs.hpp bitmap.hpp files.hpp
FILES = main.cpp disk.cpp bitmap.cpp grosfs.cpp files.cpp
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
