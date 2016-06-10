#include "Game.h"


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
		fleet[shipNum] = Ship(shipNum+1)
	}
}