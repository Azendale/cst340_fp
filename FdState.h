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
private:
	int fd;
	short state;
	std::string name;
	int otherPlayer;
};