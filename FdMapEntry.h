#pragma once

class FdMapEntry
{
public:
	FdMapEntry(int fd, short fdCollection);
	~FdMapEntry();
	int GetFD() const;
	short GetCollection() const;
	void SetCollection(short Collection);
private:
	int fd;
	short fdCollection;
};