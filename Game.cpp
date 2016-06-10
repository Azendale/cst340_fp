#include "Game.h"
#include <iostream>


Game::Game()
{
	for (int x=0; x < MAP_SIDE_SIZE; ++x)
	{
		for (int y=0; y < MAP_SIDE_SIZE; ++y)
		{
			ocean[x][y] = std::pair<Ship *, bool>(nullptr, false);
		}
	}
	
	for (int shipNum = 0; shipNum < FLEETSIZE; ++shipNum)
	{
		fleet[shipNum] = Ship(shipNum+1);
	}
}

void Game::PrintBoard() const
{
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
	}
}

void Game::CalculateMoveResults(short x, short y, bool & hit, short & shipSize, bool & sink, bool & win)
{
	if (ocean[x][y].first != nullptr && !(ocean[x][y].second))
	{
		// A new hit
		hit = true;
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
	}
	else
	{
		// Didn't hit anything
		hit = false;
		shipSize = 0;
		sink = false;
		win = false;
	}
}