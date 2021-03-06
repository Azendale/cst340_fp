#pragma once
/************************************
 * Author: Erik Andersen
 * Lab: CST340 Final Lab
 * 
 * Class that tracks the state of each connection or file descriptor in the
 * server.
 * Also includes the #defines of the various states stored in the class.
 * 
 ***********************************/

// Store things like username
#include <string>

#define FD_STATE_ACCEPT_SOCK 0
// Reads coming out of FD_STATE_ANON will be 32 bits
// Connected, reading requested name length
#define FD_STATE_ANON 1
// Reads coming out of FD_STATE_ANON_NAME_SIZE will be governed by the size sent earlier in the transition from FD_STATE_ANON to FD_STATE_ANON_NAME_SIZE
// We know the name size, reading the name is the next transition
#define FD_STATE_ANON_NAME_SIZE 2
// Waiting for someone to invite the player, or for the player to invite someone.
#define FD_STATE_LOBBY 4
// Waiting for other player to respond if they want to play or not
#define FD_STATE_REQD_GAME 5

// Waiting/reading this FD's move
#define FD_STATE_GAME_WAIT_THISFD_MOVE 6
// Waiting for other FD to give move results/writing move results to this FD
#define FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS 7
// Waiting for other player to move/writing move to this fd
#define FD_STATE_GAME_WAIT_OFD_MOVE 8
// Reading results of other connection's move from this client
#define FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS 9

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
	// Create a new state tracker for 'fd', starting in 'state'
	FdState(int fd, short state);
	// Clean up the state tracking class
	~FdState();
	// Set one state tracking class equal to another
	const FdState & operator=(const FdState & rhs);
	// Copy contruct a state class
	FdState(const FdState & s);
	// Get the Fd that is wrapped in this state class
	int GetFD() const;
	// Get the state that this connection is in
	short GetState() const;
	// Set the state that this connection is in
	void SetState(short state);
	// Set the username of the player that is connected
	void SetName(const std::string& name);
	// Get the username of the player that is connected
	std::string GetName() const;
	// Get a pointer to the other player in the game (only valid 
	// while connection is participating in a game -- otherwise nullptr)
	FdState * GetOtherPlayer() const;
	// Set the pointer to the other player in the game (only valid 
	// while connection is participating in a game -- otherwise should be reset
	// to nullptr)
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
	// Set that the last move this FD did was not a win
	void ClearLastMoveWin();
	// Set that the last move this FD did was a win
	void SetLastMoveWin();
	// See if the last move this FD did was a win
	bool GetLastMoveWin();
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
	bool lastMoveWin;
};