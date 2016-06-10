#pragma once
#include "coord.h"

class Ship
{
public:
	Ship(short size);
	Ship();
	void SetSize(short size);
	short GetSize() const;
	void Place(bool horizontal, short x, short y);
	bool Hit(short x, short y);
	bool Sunk() const;
	bool CoordIsOnShip(const coord & location) const;
	short totalSpaces;
	short hitSpaces;
	bool horizontal;
	short x;
	short y;
};