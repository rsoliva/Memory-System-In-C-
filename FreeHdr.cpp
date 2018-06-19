//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2018
// Optimized C++
//----------------------------------------------------------------------------- 

#include "Framework.h"

#include "UsedHdr.h"
#include "FreeHdr.h"
#include "BlockType.h"

// add code here
FreeHdr::FreeHdr(Type::U32 const size) {
	this->mBlockSize = size;
	this->mBlockType = (Type::U8)BlockType::FREE;
	this->mAboveBlockFree = false;
	this->pFreeNext = 0;
	this->pFreePrev = 0;
}

FreeHdr::FreeHdr(const UsedHdr &rUsed) {
	this->mAboveBlockFree = rUsed.mAboveBlockFree;
	this->mBlockSize = rUsed.mBlockSize;
	this->pFreeNext = 0;
	this->pFreePrev = 0;
	this->mBlockType = (Type::U8)BlockType::FREE;
}
// ---  End of File ---------------
