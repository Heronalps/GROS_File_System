PROJECT_NAME = grosfs
CC = g++ 
ROOT_DIR = $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
INC = $(ROOT_DIR)/include
SRC = $(ROOT_DIR)/src
CFLAGS = -Wall -m32 -g -I$(INC) -I$(SRC)

HEADERS = disk.hpp grosfs.hpp
FILES = main.cpp disk.cpp
EXECUTABLES = $(PROJECT_NAME)

all: $(EXECUTABLES)

SOURCES = $(FILES:%.cpp=$(SRC)/%.cpp)

grosfs: 
	$(CC) $(CFLAGS) -o $(PROJECT_NAME) $(SOURCES) 

clean:
	/bin/rm -rf $(wildcard *.dSYM)
	/bin/rm -f $(EXECUTABLES)
