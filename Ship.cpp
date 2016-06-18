#include "Ship.h"
/************************************
 * Author: Erik Andersen
 * Lab: CST340 Final Lab
 * 
 * Implements a class that tracks ship state in the battleship game.
 ************************************/


/****************************************************************
* Creates a ship of 'size' spaces
* 
* Preconditions:
*  None
* Postcondition:
*  Ship of 'size' spaces created. Ship placed at an invalid location on the
*  board (so it should be set with the ::Place() function)
****************************************************************/
Ship::Ship(short size): totalSpaces(size), hitSpaces(0), horizontal(false), x(-5), y(-5)
{
	
}

/****************************************************************
* Creates a ship without specifying the size
* 
* Preconditions:
*  None
* Postcondition:
*  Ship of 0 spaces created. Ship placed at an invalid location on the
*  board (so it should be set with the ::Place() function). Ship's size should
*  be set by calling ::SetSize()
****************************************************************/
Ship::Ship(): totalSpaces(0), hitSpaces(0), horizontal(false), x(-5), y(-5)
{
	
}

/****************************************************************
* Sets the size of this ship
* 
* Preconditions:
*  Ship has no hits yet and not placed yet
* Postcondition:
*  Ship set to 'size' spaces
****************************************************************/
void Ship::SetSize(short size)
{
	totalSpaces = size;
}

/****************************************************************
* Tells the ship where it is on the board. If horizontal is false, then it is
* vertically oriented
* 
* Preconditions:
*  Ship not already placed on the board, ship size set
* Postcondition:
*  Ship's location saved
****************************************************************/
void Ship::Place(bool newhorizontal, short newx, short newy)
{
	this->horizontal = newhorizontal;
	this->x = newx;
	this->y = newy;
}

/****************************************************************
* Checks if the coord is a space the ship occupies
* 
* Preconditions:
*  Ship size is set, and ship location is set
* Postcondition:
*  No changes to object, true returned if the coord is in the ship's area
****************************************************************/
bool Ship::CoordIsOnShip(const coord & location) const
{
	if (horizontal &&
		location.x >= this->x &&
		location.x < this->x+totalSpaces &&
		location.y == this->y)
	{
		return true;
	}
	else if (location.y >= this->y &&
		location.y < this->y+totalSpaces &&
		location.x == this->x)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/****************************************************************
* Checks if a shot fired at x, y would hit the ship. Ship must have had the
*  size set (either through the creation process or SetSize) and must have been
*  placed (through Place). If it is on the ship, adds a hit to the ship's hit
*  count
* Preconditions:
*  Ship size and location set.
* Postcondition:
*  records a hit if the ship is hit somewhere it has not already been hit
****************************************************************/
bool Ship::Hit(short hitx, short hity)
{
	coord location;
	location.x = hitx;
	location.y = hity;
	if (!CoordIsOnShip(location))
	{
		return false;
	}
	++hitSpaces;
	
	return true;
}

/****************************************************************
* Checks if the ship has had all spaces on it hit.
* 
* Preconditions:
*  Ship size and location has been set.
* Postcondition:
*  no changes to object. Returns true if all spaces on the ship have been hit
****************************************************************/
bool Ship::Sunk() const
{
	return hitSpaces >= totalSpaces;
}

/****************************************************************
* Retrieves the size of the ship in spaces
* 
* Preconditions:
*  Ship size set
* Postcondition:
*  No changes to object. Ship size set.
****************************************************************/
short Ship::GetSize() const
{
	return totalSpaces;
}