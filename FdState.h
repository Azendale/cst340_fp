#pragma once
#include <string>

#define FD_STATE_ACCEPT_SOCK 0
// Reads coming out of FD_STATE_ANON will be 32 bits
// Connected, reading requested name length
#define FD_STATE_ANON 1
// Reads coming out of FD_STATE_ANON_NAME_SIZE will be governed by the size sent earlier in the transition from FD_STATE_ANON to FD_STATE_ANON_NAME_SIZE
// We know the name size, reading the name is the next transition
#define FD_STATE_ANON_NAME_SIZE 2
#define FD_STATE_LOBBY 4
// Waiting for other player to respond if they want to play or not
#define FD_STATE_REQD_GAME 5

// Waiting/reading this FD's move
#define FD_STATE_GAME_WAIT_THISFD_MOVE 6
// Waiting for other FD to move/writing move results to this FD
#define FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS 7
// Waiting for other player to move/writing move to this fd
#define FD_STATE_GAME_WAIT_OFD_MOVE 8
// Reading results of other connections move from client
#define FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS 9

#define FD_STATE_WAIT_QUIT_ACK 10
// Name was rejected. Has this state during the duration of writing the reject message
#define FD_STATE_NAME_REJECT 11
// Name was accepted. Has this state during the duration of writing the accept message
#define FD_STATE_NAME_ACCEPT 12
// Names list was requested from lobby state, in the process of writing the name list
#define FD_STATE_REQ_NAME_LIST 13
// Reading name of other player to play
#define FD_STATE_OPLYR_NAME_READ 14
// Writing reject of invite/game request, after write, goes to state FD_STATE_LOBBY
#define FD_STATE_GAME_REQ_REJECT 15
// Writing accept of invite/game request, after write, gose to state FD_STATE_GAME_WAIT_OFD_MOVE
#define FD_STATE_GAME_REQ_ACCEPT 16
// Writing game invite, with other player name length followed by string as one write
#define FD_STATE_GAME_INVITE 17
// Waiting/reading response to game invitation
#define FD_STATE_GAME_INVITE_RESP_WAIT 18

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
	FdState * GetOtherPlayer() const;
	void SetOtherPlayer(FdState * other);
	// Reads once and returns true if that's all we were trying to get
	int Read();
	// Writes once and returns true if that's all we were trying to write
	int Write();
	// Set up what we want to write
	void SetWrite(const char * buff, short size);
	// Set how much we want to read
	void SetRead(short size);
	// Get what was read and how long it is
	char * GetRead(short & size);
private:
	int fd;
	short state;
	std::string name;
	FdState * otherPlayer;
	short readPtr;
	short writePtr;
	short readSize;
	short writeSize;
	char * readBuf;
	char * writeBuf;
	bool readInProgress;
	bool writeInProgress;
};