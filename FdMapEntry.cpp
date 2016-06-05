#include "FdMapEntry.h"

FdMapEntry(int fd, short fdCollection, int fdIndex)
{
	this->fd = fd;
	this->fdCollection = fdCollection;
	this->fdIndex = fdIndex;
}

~FdMapEntry()
{
	
}

int GetFD() const
{
	return fd;
}

short GetCollection() const
{
	return fdCollection;
}

int GetIndex() const
{
	return fdIndex;
}

void SetIndex(int Index)
{
	this->fdIndex = Index;
}

void SetCollection(short Collection)
{
	this->fdCollection = Collection;
}
