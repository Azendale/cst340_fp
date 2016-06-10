#**************************************************
# Makefile for battleship lab
# Erik Andersen
#
GIT_VERSION := $(shell git describe --abbrev=7 --dirty="-uncommitted" --always --tags)
CFLAGS=-Wall -Wshadow -Wunreachable-code -Wredundant-decls -DGIT_VERSION=\"$(GIT_VERSION)\" -g3 -O0 -std=gnu99
CXXFLAGS=-Wall -Wshadow -Wunreachable-code -Wredundant-decls -DGIT_VERSION=\"$(GIT_VERSION)\" -g3 -O0 -std=c++11
CXX=g++
CC=gcc

OBJS = FdState.o \
	Ship.o \
	Game.o \

all: client server

clean:
	rm -f server
	rm -f client
	rm -f *.o

.c.o:
	$(CC) $(CFLAGS) -c $? -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $? -o $@
	
server: $(OBJS) server.cpp
	$(CXX) $(CXXFLAGS) $(OBJS) server.cpp -o server

client: $(OBJS) client.cpp
	$(CXX) $(CXXFLAGS) $(OBJS) client.cpp -o client

