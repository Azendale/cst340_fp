#include "Ship.h"

Ship::Ship(short size): totalSpaces(size), hitSpaces(0), horizontal(false), x(-5), y(-5)
{
	
}

Ship::Ship(): totalSpaces(0), hitSpaces(0), horizontal(false), x(-5), y(-5)
{
	
}

void Ship::SetSize(short size)
{
	totalSpaces = size;
}

void Ship::Place(bool horizontal, short x, short y)
{
	this->horizontal = horizontal;
	this->x = x;
	this->y = y;
}

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