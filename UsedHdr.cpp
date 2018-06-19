//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2018
// Optimized C++
//----------------------------------------------------------------------------- 

#include "Framework.h"

#include "FreeHdr.h"
#include "UsedHdr.h"

// Add code here
UsedHdr::UsedHdr() {

}

UsedHdr::UsedHdr(Type::U32 size) {
	this->mBlockSize = size;
	this->mBlockType = (Type::U8)BlockType::USED;
	this->mAboveBlockFree = false;
	this->pUsedNext = 0;
	this->pUsedPrev = 0;
}
// ---  End of File ---------------
