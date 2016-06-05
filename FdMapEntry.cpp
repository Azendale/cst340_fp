#include "FdMapEntry.h"

FdMapEntry::FdMapEntry(int fd, short fdCollection, int fdIndex)
{
	this->fd = fd;
	this->fdCollection = fdCollection;
	this->fdIndex = fdIndex;
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

int FdMapEntry::GetIndex() const
{
	return fdIndex;
}

void FdMapEntry::SetIndex(int Index)
{
	this->fdIndex = Index;
}

void FdMapEntry::SetCollection(short Collection)
{
	this->fdCollection = Collection;
}
