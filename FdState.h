#pragma once
#include <string>

#define FD_STATE_ACCEPT_SOCK 0
#define FD_STATE_ANON 1
#define FD_STATE_NAME_REQUESTED 2
#define FD_STATE_LOBBY 3
#define FD_STATE_REQ_GAME 4
#define FD_STATE_GAME_WAIT_THISFD_MOVE 5
#define FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS 6
#define FD_STATE_GAME_WAIT_OFD_MOVE 7
#define FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS 8
#define FD_STATE_WAIT_QUIT_ACK 9

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
	void GetRead(char * & buff, short & size);
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