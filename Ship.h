#pragma once
#include "coord.h"

class Ship
{
public:
	Ship(int size);
	void Place(bool horizontal, short x, short y);
	bool CoordIsOnShip(const coord & location) const;
	int totalSpaces;
	int hitSpaces;
	bool horizontal;
	short x;
	short y;
};