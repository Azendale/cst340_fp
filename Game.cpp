#include "Game.h"
#include <iostream>

/****************************************************************
* Create a new game object
* Preconditions:
*  None
* Postcondition:
*  Game board initialized to empty boards. Ships not placed.
****************************************************************/
Game::Game()
{
	for (int x=0; x < MAP_SIDE_SIZE; ++x)
	{
		for (int y=0; y < MAP_SIDE_SIZE; ++y)
		{
			ocean[x][y] = std::pair<Ship *, bool>(nullptr, false);
			targetmap[x][y] = std::pair<bool, bool>(false, false);
		}
	}
	
	for (int shipNum = 0; shipNum < FLEETSIZE; ++shipNum)
	{
		fleet[shipNum] = Ship(shipNum+1);
	}
}

/****************************************************************
* Records where we sucessfully hit another ship on the other player's board
* Preconditions:
*  x and y < MAP_SIDE_SIZE
* Postcondition:
*  That place on the map marked as a sucessfull hit
****************************************************************/
void Game::SetPlayHitCoord(short x, short y)
{
	targetmap[x][y].second = true;
}

/****************************************************************
* Marks where the user targeted on the other player's map
* Preconditions:
*  x and y < MAP_SIDE_SIZE
* Postcondition:
*  That place on the map marked as having been fired at
****************************************************************/
void Game::SetPlayCoord(short x, short y)
{
	targetmap[x][y].first = true;
}

/*****************************************************************
* Displays the board to stdout (both the ocean and targeting boards)
* Preconditions:
*  None?
* Postcondition:
*  Targeting and ocean boards printed out to stdout
****************************************************************/
void Game::PrintBoard() const
{
	std::cout << "Target Map:\n";
	for (int x=1; x <= MAP_SIDE_SIZE; ++x)
	{
		std::cout << x << " ";
	}
	std::cout << "\n\033[37m"; // Use white to draw
	for (int y=0; y < MAP_SIDE_SIZE; ++y)
	{
		std::cout << "\033[1;37m"; // Bold for the playing field
		for (int x=0; x < MAP_SIDE_SIZE; ++x)
		{
			if (targetmap[x][y].first)
			{
				// Ship there
				if (targetmap[x][y].second)
				{
					std::cout << "\033[31m"; // red for hits
					// This square fired at
					std::cout << "<>";
					std::cout << "\033[37m"; // Reset color after hit -- back to white
				}
				else
				{
					// This square not fired at
					std::cout << "<>";
				}
			}
			else
			{
				// This square not fired at
				std::cout << "  ";
			}
		}
		std::cout << "\033[0m  - " << y+1 << "\n"; // Unbold and then put numbers
	}
	std::cout << "\033[0m"; // Reset everything to defaults	
	std::cout << "Ocean:\n";
	for (int x=1; x <= MAP_SIDE_SIZE; ++x)
	{
		std::cout << x << " ";
	}
	std::cout << "\n\033[37m"; // Use white to draw
	for (int y=0; y < MAP_SIDE_SIZE; ++y)
	{
		std::cout << "\033[1;37m"; // Bold for the playing field
		for (int x=0; x < MAP_SIDE_SIZE; ++x)
		{
			if (ocean[x][y].first != nullptr)
			{
				std::cout << "\033[7m"; // Negative image for ships
				// Ship there
				if (ocean[x][y].second)
				{
					std::cout << "\033[31m"; // red for hits
					// This square fired at
					std::cout << "<>";
					std::cout << "\033[37m"; // Reset color after hit -- back to white
				}
				else
				{
					// This square not fired at
					std::cout << "  ";
				}
				std::cout << "\033[27m"; // Reset to positive image
			}
			else
			{
				// No ship there
				if (ocean[x][y].second)
				{
					// This square fired at
					std::cout << "<>";
				}
				else
				{
					// This square not fired at
					std::cout << "  ";
				}
			}
		}
		std::cout << "\033[0m  - " << y+1 << "\n"; // Unbold and then put numbers
	}
	std::cout << "\033[0m"; // Reset everything to defaults
}

/***************************************************************
*Interacts with the user through stdin to place the ships on their board
* Preconditions:
*  Ships not already placed
* Postcondition:
*  Ships placed on the board
****************************************************************/
void Game::PlaceShips()
{
	for (int i=5; i > 0; --i)
	{
		std::cout << "Please select where you want to place the " << i << "-space ship:\n";
		char horizC = 'u';
		bool horiz;
		for (; horizC != 'y' && horizC != 'Y' && horizC != 'n' && horizC != 'N';)
		{
			std::cout << "Do you want the ship in a horizontal position? (If you pick no, it will be vertical.) [y/n]: ";
			std::cin >> horizC;
		}
		if ('Y' == horizC || 'y' == horizC)
		{
			horiz = true;
		}
		else
		{
			horiz = false;
		}
		int newx = 0;
		int newy = 0;
		if (horiz)
		{
			for (; newx < 1 || newx > MAP_SIDE_SIZE-i+1;)
			{
				std::cout << "Enter an x coordinate (must be in the range " << 1 << ".." << MAP_SIDE_SIZE-i+1 << " inclusive): ";
				std::cin >> newx;
			}
			for (; newy < 1 || newy > MAP_SIDE_SIZE;)
			{
				std::cout << "Enter an y coordinate (must be in the range " << 1 << ".." << MAP_SIDE_SIZE << " inclusive): ";
				std::cin >> newy;
			}
		}
		else // vertical
		{
			for (; newx < 1 || newx > MAP_SIDE_SIZE;)
			{
				std::cout << "Enter an x coordinate (must be in the range " << 1 << ".." << MAP_SIDE_SIZE << " inclusive): ";
				std::cin >> newx;
			}
			for (; newy < 1 || newy > MAP_SIDE_SIZE-i+1;)
			{
				std::cout << "Enter an y coordinate (must be in the range " << 1 << ".." << MAP_SIDE_SIZE-i+1 << " inclusive): ";
				std::cin >> newy;
			}
		}
		// Get to 0 based numbering
		newx -= 1;
		newy -= 1;
		fleet[i-1].Place(horiz, newx, newy);
		if (horiz)
		{
			for (int j = newx; j < i+newx; ++j)
			{
				ocean[j][newy].first = &(fleet[i-1]);
			}
		}
		else
		{
			for (int j = newy; j < i+newy; ++j)
			{
				ocean[newx][j].first = &(fleet[i-1]);
			}
		}
		this->PrintBoard();
	}
}

/*****************************************************************
* Calulates the results of the other player's move at x,y. 'shipSize' is
* the size of the ship that was hit (if any). 'win' is true if they just
* won. 'sink' is if they sunk a ship. 'hit' is if they hit a ship.
* 
* Preconditions:
*  x and y < MAP_SIDE_SIZE
* Postcondition:
*  Game board updated to show the results of firing on that location.
*  hit set to true if a ship was hit in a new location, otherwise false
*  shipSize set to true if a new hit happened, otherwise just left
*  sink set to true if a ship was sunk by the shot, otherwise set to false
*  win set to true if that move just won the game
****************************************************************/
void Game::CalculateMoveResults(short x, short y, bool & hit, short & shipSize, bool & sink, bool & win)
{
	if (ocean[x][y].first != nullptr && !(ocean[x][y].second))
	{
		// A new hit
		ocean[x][y].second = hit = true;
		shipSize = ocean[x][y].first->GetSize();
		// Did they sink a boat?
		ocean[x][y].first->Hit(x, y);
		if (ocean[x][y].first->Sunk())
		{
			sink = true;
			// Check if that sunk all of the fleet
			bool allHit = true;
			for (int i = 0; i < FLEETSIZE; ++i)
			{
				if (!(fleet[i].Sunk()))
				{
					// This ship is still there
					allHit = false;
				}
			}
			win = allHit;
		}
		else
		{
			sink = false;
		}
	}
	else
	{
		// Didn't hit anything
		hit = false;
		shipSize = 0;
		sink = false;
		win = false;
		// Except ocean
		ocean[x][y].second = true;
	}
}