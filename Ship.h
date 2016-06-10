#pragma once
#include "coord.h"

class Ship
{
public:
	Ship(short size);
	Ship();
	void SetSize(short size);
	void Place(bool horizontal, short x, short y);
	bool CoordIsOnShip(const coord & location) const;
	short totalSpaces;
	short hitSpaces;
	bool horizontal;
	short x;
	short y;
};