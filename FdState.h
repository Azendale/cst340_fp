#pragma once
#include <string>

class FdState
{
public:
	FdState(int fd, short state);
	~FdState();
	int GetFD() const;
	short GetState() const;
	void SetName();
	std::string GetName() const;
private:
	int fd;
	short state;
	std::string name;
	
};