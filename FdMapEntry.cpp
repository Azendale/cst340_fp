#include "FdMapEntry.h"

FdMapEntry::FdMapEntry(int fd, short fdCollection)
{
	this->fd = fd;
	this->fdCollection = fdCollection;
}

FdMapEntry::~FdMapEntry()
{
	
}

int FdMapEntry::GetFD() const
{
	return fd;
}

short FdMapEntry::GetCollection() const
{
	return fdCollection;
}

void FdMapEntry::SetCollection(short Collection)
{
	this->fdCollection = Collection;
}
