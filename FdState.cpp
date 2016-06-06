#include "FdState.h"

FdState::FdState(int Fd, short State)
{
	this->fd = Fd;
	this->state = State;
	this->otherPlayer = -1;
}

FdState::~FdState()
{
	
}

int FdState::GetFD() const
{
	return fd;
}

short FdState::GetState() const
{
	return state;
}

void FdState::SetState(short State)
{
	this->state = State;
}

void FdState::SetName(const std::string & newName)
{
	this->name = newName;
}

std::string FdState::GetName() const
{
	return this->name;
}

int FdState::GetOtherPlayer() const
{
	return otherPlayer;
}

void FdState::SetOtherPlayer(int newOtherPlayer)
{
	this->otherPlayer = newOtherPlayer;
}