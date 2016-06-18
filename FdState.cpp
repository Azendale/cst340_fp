#include "FdState.h"
#include <cstring>
extern "C"
{
	#include <unistd.h>
}

/***************************************************************
* Create a new state tracker for 'fd', starting in 'state'
* 
* Preconditions:
*  None
* Postcondition:
*  Fd state tracker created, with no reads/writes in progress
****************************************************************/
FdState::FdState(int Fd, short State): fd(Fd), state(State), name(""), otherPlayer(nullptr), readPtr(-1), writePtr(-1), readSize(0), writeSize(0), readBuf(nullptr), writeBuf(nullptr), readInProgress(false), writeInProgress(false), lastMoveWin(false)
{
	
}

/***************************************************************
* Copy contruct a state class
* 
* Preconditions:
*  's' is a valid FdState object
* Postcondition:
*  *this is a copy of 's'
****************************************************************/
FdState::FdState(const FdState & s): fd(s.fd), state(s.state), name(s.name), otherPlayer(s.otherPlayer), readPtr(s.readPtr), writePtr(s.writePtr), readSize(s.readSize), writeSize(s.writeSize), readBuf(nullptr), writeBuf(nullptr), readInProgress(s.readInProgress), writeInProgress(s.writeInProgress), lastMoveWin(s.lastMoveWin)
{
	// deep copy these two
	//char * readBuf;
	//char * writeBuf;
	if (s.readBuf)
	{
		this->readBuf = new char[this->readSize];
		memmove(this->readBuf, s.readBuf, this->readSize);
	}
	if (s.writeBuf)
	{
		this->writeBuf = new char[this->writeSize];
		memmove(this->writeBuf, s.writeBuf, this->writeSize);
	}
}

/***************************************************************
* Set one state tracking class equal to another
* 
* Preconditions:
*  rhs a valid FdState object
* Postcondition:
*  *this set to be the same as (but independent of) rhs
****************************************************************/
const FdState & FdState::operator=(const FdState & rhs)
{
	this->fd = rhs.fd;
	this->state = rhs.state;
	this->name = rhs.name;
	this->otherPlayer = rhs.otherPlayer;
	this->readPtr = rhs.readPtr;
	this->writePtr = rhs.writePtr;
	this->readSize = rhs.readSize;
	this->writeSize = rhs.writeSize;
	this->readInProgress = rhs.readInProgress;
	this->writeInProgress = rhs.writeInProgress;
	this->lastMoveWin = rhs.lastMoveWin;
	// If any memory is currently allocated, free it
	if (this->readBuf)
	{
		delete [] this->readBuf;
		this->readBuf = nullptr;
	}
	// If any memory is currently allocated, free it
	if (this->writeBuf)
	{
		delete [] this->writeBuf;
		this->writeBuf = nullptr;
	}
	// If any memory is currently allocated in the source, copy it
	if (rhs.readBuf)
	{
		this->readBuf = new char[this->readSize];
		memmove(this->readBuf, rhs.readBuf, this->readSize);
	}
	// If any memory is currently allocated in the source, copy it
	if (rhs.writeBuf)
	{
		this->writeBuf = new char[this->writeSize];
		memmove(this->writeBuf, rhs.writeBuf, this->writeSize);
	}
	return *this;
}

/***************************************************************
* Clean up the state tracking class
* 
* Preconditions:
*  object no longer in use and will not be used in the future
* Postcondition:
*  Memory used by object freed
****************************************************************/
FdState::~FdState()
{
	readInProgress = false;
	readSize = 0;
	// delete on nullptr is safe so no check
	delete [] readBuf;
	readBuf = nullptr;
	readPtr = -1;
	writeInProgress = false;
	writeSize = 0;
	// delete on nullptr is safe so no check
	delete [] writeBuf;
	writeBuf = nullptr;
	writePtr = -1;
}

/***************************************************************
* Set that the last move this FD did was not a win
* 
* Preconditions:
*  None
* Postcondition:
*  state marked as having the last move not be a win
****************************************************************/
void FdState::ClearLastMoveWin()
{
	lastMoveWin = false;
}

/***************************************************************
* Set that the last move this FD did was a win
* 
* Preconditions:
*  none
* Postcondition:
*  state marked as having the last move be a win
****************************************************************/
void FdState::SetLastMoveWin()
{
	lastMoveWin = true;
}

/***************************************************************
* See if the last move this FD did was a win
* 
* Preconditions:
*  None 
* Postcondition:
*  find out if the last move was marked as being a win
****************************************************************/
bool FdState::GetLastMoveWin()
{
	return lastMoveWin;
}

/***************************************************************
* Get the Fd that is wrapped in this state class
* 
* Preconditions:
*  None
* Postcondition:
*  No object changes, but return the Fd this object wraps
****************************************************************/
int FdState::GetFD() const
{
	return fd;
}

/***************************************************************
* Get the state that this connection is in
* 
* Preconditions:
*  None
* Postcondition:
*  No object changes, but return the state this connection is in
****************************************************************/
short FdState::GetState() const
{
	return state;
}

/***************************************************************
* Set the state that this connection is in
* 
* Preconditions:
*  'State' is one of the #defined state in FdState.h
* Postcondition:
*  object set to be in the specified state
****************************************************************/
void FdState::SetState(short State)
{
	this->state = State;
}

/***************************************************************
* Set the username of the player that is connected
* 
* Preconditions:
*  None
* Postcondition:
*  saved player name updated.
****************************************************************/
void FdState::SetName(const std::string & newName)
{
	this->name = newName;
}

/***************************************************************
* Get the username of the player that is connected
* 
* Preconditions:
*  Player name has been previously set
* Postcondition:
*  Player name returned
****************************************************************/
std::string FdState::GetName() const
{
	return this->name;
}

/***************************************************************
* Get a pointer to the other player in the game (only valid 
* while connection is participating in a game -- otherwise nullptr)
* 
* Preconditions:
*  Connection is participating in a game (or the caller is checking for a
*  nullptr return)
* Postcondition:
*  No object changes, pointer to other player object returned or nullptr
****************************************************************/
FdState * FdState::GetOtherPlayer() const
{
	return otherPlayer;
}

/***************************************************************
* Set the pointer to the other player in the game (only valid 
* while connection is participating in a game -- otherwise should be reset
* to nullptr)
* 
* Preconditions:
*  other a valid pointer to another connection participating in a game (or null)
* Postcondition:
*  saved other player connection pointer updated
****************************************************************/
void FdState::SetOtherPlayer(FdState * other)
{
	this->otherPlayer = other;
}

/***************************************************************
* Reads once and returns true if that's all we were trying to get
* 
* Preconditions:
*  SetRead called since the last time this returned 1
* Postcondition:
*  returns 0 if some, but not all data was successfully read
*  returns 1 if the rest of the data was successfully read
*  returns -2 if we hit the end of the file
*  returns -3 of there was some other error
****************************************************************/
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

/***************************************************************
* Writes once and returns true if that's all we were trying to write
* 
* Preconditions:
*  SetWrite called since the last time this returned 1
*  
* Postcondition:
*  returns 0 if some, but not all data was successfully written
*  returns 1 if the rest of the data was successfully written
*  returns -2 if we hit the end of the file
*  returns -3 of there was some other error
****************************************************************/
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

/***************************************************************
* Set up what we want to write
* 
* Preconditions:
*  Write not already in progress, 'size' the number of bytes to transfer, 'buff'
*   a pointer to the data to write
* Postcondition:
*  Connection set up to be ready to write data with ::Write(), data in 'buff'
*  copied to internal buffer
****************************************************************/
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

/***************************************************************
* Set how much we want to read
* 
* Preconditions:
*  Read not already in progress, 'size' the number of bytes to transfer
* Postcondition:
*  Connection set up to be ready to write data with ::Read()
****************************************************************/
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

/***************************************************************
* Get what was read and how long it is
* 
* Preconditions:
*  Read() returned -1 since the last time this was called
* Postcondition:
*  'size' updated to the size of that was read
*   returns pointer to the data. Warning: pointer invalidated at next read or
*   class destruction
****************************************************************/
char * FdState::GetRead(short & size)
{
	size = readSize;
	return readBuf;
}