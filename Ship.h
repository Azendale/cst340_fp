#pragma once
/************************************
 * Author: Erik Andersen
 * Lab: CST340 Final Lab
 * 
 * class Ship:
 * represents a ship in the battleship game.
 * 
 *
 * Ship(short size)
 *  Creates a ship of 'size' spaces
 * Ship()
 *  Creates a ship without specifying the size
 * GetSize() const
 *  Retrieves the size of the ship in spaces
 * void Place(bool horizontal, short x, short y)
 *  Tells the ship where it is on the board. If horizontal is false, then it is
 *  vertically oriented
 * bool Hit(short x, short y)
 *  Checks if a shot fired at x, y would hit the ship. Ship must have had the
 *  size set (either through the creation process or SetSize) and must have been
 *  placed (through Place). If it is on the ship, adds a hit to the ship's hit
 *  count
 * bool Sunk() const
 *  Checks if the ship has had all spaces on it hit.
 * bool CoordIsOnShip(const coord & location) const
 *  Checks if the coord is a space the ship occupies
 ***********************************/

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
protected:
	short totalSpaces;
	short hitSpaces;
	bool horizontal;
	short x;
	short y;
};