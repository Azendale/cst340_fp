#pragma once
#include <string>

#define FD_STATE_ACCEPT_SOCK 0
// Reads coming out of FD_STATE_ANON will be 32 bits
// Connected, reading requested name length
#define FD_STATE_ANON 1
// Reads coming out of FD_STATE_ANON_NAME_SIZE will be governed by the size sent earlier in the transition from FD_STATE_ANON to FD_STATE_ANON_NAME_SIZE
// We know the name size, reading the name is the next transition
#define FD_STATE_ANON_NAME_SIZE 2
// Writes to FD_STATE_NAME_REQUESTED will be 32 bits

#define FD_STATE_NAME_REQUESTED 3
#define FD_STATE_LOBBY 4
#define FD_STATE_REQ_GAME 5
#define FD_STATE_GAME_WAIT_THISFD_MOVE 6
#define FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS 7
#define FD_STATE_GAME_WAIT_OFD_MOVE 8
#define FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS 9
#define FD_STATE_WAIT_QUIT_ACK 10
// Name was rejected. Has this state during the duration of writing the reject message
#define FD_STATE_NAME_REJECT 11
// Name was accepted. Has this state during the duration of writing the accept message
#define FD_STATE_NAME_ACCEPT 12

class FdState
{
public:
	FdState(int fd, short state);
	~FdState();
	int GetFD() const;
	short GetState() const;
	void SetState(short state);
	void SetName(const std::string& name);
	std::string GetName() const;
	int GetOtherPlayer() const;
	void SetOtherPlayer(int newOtherPlayer);
	// Reads once and returns true if that's all we were trying to get
	int Read();
	// Writes once and returns true if that's all we were trying to write
	int Write();
	// Set up what we want to write
	void SetWrite(char * buff, short size);
	// Set how much we want to read
	void SetRead(short size);
	// Get what was read and how long it is
	char * GetRead(short & size);
private:
	int fd;
	short state;
	std::string name;
	int otherPlayer;
	short readPtr;
	short writePtr;
	short readSize;
	short writeSize;
	char * readBuf;
	char * writeBuf;
	bool readInProgress;
	bool writeInProgress;
};