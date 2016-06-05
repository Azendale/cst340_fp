#pragma once

class FdMapEntry
{
public:
	FdMapEntry(int fd, short fdCollection, int fdIndex);
	~FdMapEntry();
	int GetFD() const;
	short GetCollection() const;
	int GetIndex() const;
	void SetIndex(int Index);
	void SetCollection(short Collection);
private:
	int fd;
	short fdCollection;
	int fdIndex;
};