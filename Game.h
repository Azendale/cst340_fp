#pragma once
#include "Ship.h"
#include <utility>
#define MAP_SIDE_SIZE 10
#define FLEETSIZE 5

class Game
{
public:
	Game();
	void PrintBoard() const;
	void PlaceShips();
	void SetPlayHitCoord(short x, short y);
	void SetPlayCoord(short x, short y);
	void CalculateMoveResults(short x, short y, bool & hit, short & shipSize, bool & sink, bool & win);
	std::pair<Ship *, bool> ocean[MAP_SIDE_SIZE][MAP_SIDE_SIZE];
	std::pair<bool, bool> targetmap[MAP_SIDE_SIZE][MAP_SIDE_SIZE];
	Ship fleet[FLEETSIZE];
};