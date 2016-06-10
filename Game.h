#pragma once
#include "Ship.h"
#define MAP_SIDE_SIZE 10
#define FLEETSIZE 5

class Game
{
public:
	Game();
	std::pair<Ship *, bool> ocean[MAP_SIDE_SIZE][MAP_SIDE_SIZE];
	Ship fleet[FLEETSIZE];
};