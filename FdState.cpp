#include "FdState.h"
extern "C"
{
	#include <unistd.h>
}

FdState::FdState(int Fd, short State): fd(Fd), state(State), name(""), otherPlayer(nullptr), readPtr(-1), writePtr(-1), readSize(0), writeSize(0), readBuf(nullptr), writeBuf(nullptr), readInProgress(false), writeInProgress(false)
{
	
}

FdState::~FdState()
{
	readInProgress = false;
	readSize = 0;
	delete [] readBuf;
	readBuf = nullptr;
	readPtr = -1;
	writeInProgress = false;
	writeSize = 0;
	delete [] writeBuf;
	writeBuf = nullptr;
	writePtr = -1;
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

FdState * FdState::GetOtherPlayer() const
{
	return otherPlayer;
}

void FdState::SetOtherPlayer(FdState * other)
{
	this->otherPlayer = other;
}

// Reads once and returns 
int FdState::Read()
{
	int readCount = read(fd, readBuf+readPtr, readSize-readPtr);
	if (readCount > 0)
	{
		readPtr += readCount;
		if (readPtr == readSize)
		{
			readInProgress = false;
			return 1;
		}
		return 0;
	}
	else if (readCount == 0)
	{
		// End of file
		return -2;
	}
	else
	{
		// Error
		return -3;
	}
}

// Writes once and returns
int FdState::Write()
{
	int writeCount = write(fd, writeBuf+writePtr, writeSize-writePtr);
	if (writeCount > 0)
	{
		writePtr += writeCount;
		if (writePtr == writeSize)
		{
			writeInProgress = false;
			return 1;
		}
		return 0;
	}
	else if (writeCount == 0)
	{
		// End of file
		return -2;
	}
	else
	{
		// Error
		return -3;
	}
}

// Set up what we want to write
void FdState::SetWrite(const char * buff, short size)
{
	if (size > 0)
	{
		writeSize = size;
		if (writeBuf != nullptr)
		{
			delete [] writeBuf;
		}
		writeBuf = new char[size];
		writePtr = 0;
		for (int i=0; i < size; ++i)
		{
			writeBuf[i] = buff[i];
		}
	}
}

// Set how much we want to read
void FdState::SetRead(short size)
{
	if (size > 0)
	{
		readSize = size;
		if (readBuf != nullptr)
		{
			delete [] readBuf;
		}
		readBuf = new char[size];
		readPtr = 0;
	}
}

// Get what was read and how long it is
char * FdState::GetRead(short & size)
{
	size = readSize;
	return readBuf;
}