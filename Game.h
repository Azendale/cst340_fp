#pragma once
/************************************
 * Author: Erik Andersen
 * Lab: CST340 Final Lab
 * 
 * class Game:
 *  Encapsulates the boards (target and ocean) and the ships.
 ***********************************/

#include "Ship.h"
#include <utility>
#define MAP_SIDE_SIZE 10
#define FLEETSIZE 5

class Game
{
public:
	// Create a new game object
	Game();
	// Displays the board to stdout (both the ocean and targeting boards)
	void PrintBoard() const;
	// Interacts with the user through stdin to place the ships on their board
	void PlaceShips();
	// Records where we sucessfully hit another ship on the other player's board
	void SetPlayHitCoord(short x, short y);
	// Marks where the user targeted on the other player's map
	void SetPlayCoord(short x, short y);
	// Calulates the results of the other player's move at x,y. 'shipSize' is
	// the size of the ship that was hit (if any). 'win' is true if they just
	// won. 'sink' is if they sunk a ship. 'hit' is if they hit a ship.
	void CalculateMoveResults(short x, short y, bool & hit, short & shipSize,
	bool & sink, bool & win);
protected:
	std::pair<Ship *, bool> ocean[MAP_SIDE_SIZE][MAP_SIDE_SIZE];
	std::pair<bool, bool> targetmap[MAP_SIDE_SIZE][MAP_SIDE_SIZE];
	Ship fleet[FLEETSIZE];
};