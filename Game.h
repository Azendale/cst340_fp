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
	std::pair<Ship *, bool> ocean[MAP_SIDE_SIZE][MAP_SIDE_SIZE];
	Ship fleet[FLEETSIZE];
};