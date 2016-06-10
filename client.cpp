#include <iostream>
#include "Game.h"

int main()
{
Game test;
test.ocean[2][2].second = true;
test.PrintBoard();
test.PlaceShips();
test.PrintBoard();
return 0;
}
