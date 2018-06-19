//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2018
// Optimized C++
//----------------------------------------------------------------------------- 

#include <malloc.h>
#include <new>

#include "Framework.h"

#include "Mem.h"
#include "Heap.h"
#include "BlockType.h"

#define STUB_PLEASE_REPLACE(x) (x)

#define HEAP_ALIGNMENT			16
#define HEAP_ALIGNMENT_MASK		(HEAP_ALIGNMENT-1)

#define ALLOCATION_ALIGNMENT		16
#define ALLOCATION_ALIGNMENT_MASK	(ALLOCATION_ALIGNMENT-1)

#define UNUSED_VAR(v)  ((void *)v)

#ifdef _DEBUG
	#define HEAP_HEADER_GUARDS  16
	#define HEAP_SET_GUARDS  	Type::U32 *pE = (Type::U32 *)((Type::U32)pRawMem + HEAP_SIZE); \
								*pE++ = 0xEEEEEEEE;*pE++ = 0xEEEEEEEE;*pE++ = 0xEEEEEEEE;*pE++ = 0xEEEEEEEE;
	#define HEAP_TEST_GUARDS	Type::U32 *pE = (Type::U32 *)((Type::U32)pRawMem + HEAP_SIZE); \
								assert(*pE++ == 0xEEEEEEEE);assert(*pE++ == 0xEEEEEEEE); \
								assert(*pE++ == 0xEEEEEEEE);assert(*pE++ == 0xEEEEEEEE);  
#else
	#define HEAP_HEADER_GUARDS  0
	#define HEAP_SET_GUARDS  	
	#define HEAP_TEST_GUARDS			 
#endif
							

// To help with coalescing... not required
struct SecretPtr
{
	FreeHdr *free;
};


Mem::~Mem()
{
	HEAP_TEST_GUARDS
	_aligned_free(this->pRawMem);
}


Heap *Mem::GetHeap()
{
	return this->pHeap;
}

Mem::Mem()
{
	// now initialize it.
	this->pHeap = 0;
	this->pRawMem = 0;

	// Do a land grab --- get the space for the whole heap
	// Since OS have different alignments... I forced it to 16 byte aligned
	pRawMem = _aligned_malloc(HEAP_SIZE + HEAP_HEADER_GUARDS, HEAP_ALIGNMENT);
	HEAP_SET_GUARDS

	// verify alloc worked
	assert(pRawMem != 0);

	// Guarantee alignemnt
	assert( ((Type::U32)pRawMem & HEAP_ALIGNMENT_MASK) == 0x0 ); 

	// instantiate the heap header on the raw memory
	Heap *p = new(pRawMem) Heap(pRawMem);

	// update it
	this->pHeap = p;
}


void Mem::Initialize()
{
	// Add magic here
	FreeHdr *tmp = reinterpret_cast<FreeHdr*>((Type::U8*)this->pHeap + sizeof(Heap));
	this->pHeap->pFreeHead = tmp;
	this->pHeap->pNextFit = pHeap->pFreeHead;
	this->pHeap->mStats.currNumFreeBlocks++;

	//finds free memory
	unsigned int freeRem;
	unsigned int bottom = (Type::U32)this->pHeap->mStats.heapBottomAddr;
	unsigned int bottomHdr = (Type::U32)this->pHeap->pFreeHead + sizeof(*this->pHeap->pFreeHead);
	freeRem = bottom - bottomHdr;

	//makes that free memory part of a free block
	this->pHeap->pFreeHead = new(tmp)FreeHdr(freeRem);
	this->pHeap->mStats.currFreeMem = freeRem;
	this->pHeap->pUsedHead = 0;
}


void *Mem::Malloc( const Type::U32 size )
{
	this->pHeap->mStats.currNumUsedBlocks++;
	if (this->pHeap->mStats.currNumUsedBlocks > this->pHeap->mStats.peakNumUsed)
		this->pHeap->mStats.peakNumUsed = this->pHeap->mStats.currNumUsedBlocks;
	this->pHeap->mStats.currUsedMem += size;
	if (this->pHeap->mStats.currUsedMem > this->pHeap->mStats.peakUsedMemory)
		this->pHeap->mStats.peakUsedMemory = this->pHeap->mStats.currUsedMem;

	FreeHdr *currFH = this->pHeap->pNextFit;
	FreeHdr *dest = this->pHeap->pNextFit;
	while (currFH != 0) {
		if (currFH->mBlockSize >= size) {
			unsigned int foundBlockSize = currFH->mBlockSize;
			FreeHdr *nextF = currFH->pFreeNext;
			FreeHdr *prevF = currFH->pFreePrev;

			UsedHdr *pAddr = (UsedHdr*)currFH;
			UsedHdr *newUH = new(pAddr)UsedHdr(size);
			FreeHdr *below = (FreeHdr*)((Type::U32)newUH + sizeof(UsedHdr) + newUH->mBlockSize);
			if ((Type::U32)below != (Type::U32)this->pHeap->mStats.heapBottomAddr)
				below->mAboveBlockFree = false;
			if(this->pHeap->pUsedHead == 0)
				this->pHeap->pUsedHead = newUH;
			else {
				UsedHdr *ptmp = this->pHeap->pUsedHead;
				this->pHeap->pUsedHead = newUH; //place in front
				//fix links
				newUH->pUsedPrev = 0;
				newUH->pUsedNext = ptmp;
				ptmp->pUsedPrev = newUH;
			}

			Type::U32 sizeOfFree = foundBlockSize - size;
			if (sizeOfFree > (2 * sizeof(FreeHdr))) {
				Type::U32 newSize = sizeOfFree - sizeof(FreeHdr);
				FreeHdr *pSub = new(below)FreeHdr(newSize);

				//preserve links and blocktype
				pSub->pFreeNext = nextF;
				pSub->pFreePrev = prevF;
				if (prevF != 0)
					prevF->pFreeNext = pSub;
				if (nextF != 0)
					nextF->pFreePrev = pSub;
				//update next fix
				this->pHeap->pNextFit = pSub;

				if (pSub->pFreePrev == 0)
					this->pHeap->pFreeHead = pSub;

				//set secret pointer
				Type::U32 hdrStart = (Type::U32)pSub;
				Type::U32 hdrEnd = hdrStart + sizeof(FreeHdr);
				Type::U32 blkEnd = hdrEnd + pSub->mBlockSize;
				Type::U32 secret = (Type::U32)pSub;
				Type::U32 *sPointer = (Type::U32*)(blkEnd - 4);
				*sPointer = secret;

				this->pHeap->mStats.currFreeMem -= size;
				this->pHeap->mStats.currFreeMem -= sizeof(FreeHdr);
			}
			else {
				this->pHeap->mStats.currNumFreeBlocks--;
				this->pHeap->mStats.currFreeMem -= size;
				//fix fh list links
				if (prevF != 0) {
					currFH->pFreePrev = prevF;
					currFH->pFreePrev->pFreeNext = nextF;
				}	
				if (nextF != 0) {
					currFH->pFreeNext = nextF;
					currFH->pFreeNext->pFreePrev = prevF;
				}
				if (prevF == 0) 
					this->pHeap->pFreeHead = nextF;
				this->pHeap->pNextFit = nextF;
			}
			break;
		}
		currFH = currFH->pFreeNext;
		if (currFH == 0)
			currFH = this->pHeap->pFreeHead;
		if (currFH == dest)
			break;
	}

	void* p;
	p = this->pHeap->pUsedHead + 1;
	return p;
}

FreeHdr* Mem::combineBelow(FreeHdr *newFH, FreeHdr *r) {
	if (this->pHeap->pNextFit == r)
		this->pHeap->pNextFit = newFH;

	FreeHdr *pNext = r->pFreeNext;
	FreeHdr *pPrev = r->pFreePrev;

	unsigned int newSize = r->mBlockSize + sizeof(FreeHdr) + newFH->mBlockSize;
	//store to newly freed block
	newFH->mBlockSize = newSize;
	newFH->pFreeNext = pNext;
	newFH->pFreePrev = pPrev;

	if (newFH->pFreePrev == 0)
		this->pHeap->pFreeHead = newFH;
	else
		newFH->pFreePrev->pFreeNext = newFH;
	if (newFH->pFreeNext != 0)
		newFH->pFreeNext->pFreePrev = newFH;
	//r->pFreeNext = 0;
	//r->pFreePrev = 0;
	
	return newFH;
}
FreeHdr* Mem::combineAbove(FreeHdr *newFH, FreeHdr *above, boolean cbelow) {
	if (this->pHeap->pNextFit == newFH)
		this->pHeap->pNextFit = above;
	newFH->pFreeNext = 0;
	newFH->pFreePrev = 0;
	FreeHdr *pPrev = above->pFreePrev;
	FreeHdr *pNext;
	if (cbelow)
		pNext = newFH->pFreeNext;
	else
		pNext = above->pFreeNext;
	unsigned int aSize = above->mBlockSize;
	unsigned int nfhSize = newFH->mBlockSize;
	unsigned int newSize = aSize + nfhSize + sizeof(FreeHdr);

	newFH = above;
	newFH->mBlockSize = newSize;
	newFH->pFreePrev = pPrev;
	newFH->pFreeNext = pNext;
	if (newFH->pFreePrev == 0)
		this->pHeap->pFreeHead = newFH;
	else
		newFH->pFreePrev->pFreeNext = newFH;
	if (newFH->pFreeNext != 0)
		newFH->pFreeNext->pFreePrev = newFH;

	return newFH;
}

void Mem::insert(FreeHdr *newFH){
	FreeHdr *currFB = this->pHeap->pFreeHead;
	if (currFB == 0) {
		this->pHeap->pFreeHead = newFH;
		this->pHeap->pNextFit = newFH;
		newFH->pFreePrev = 0;
	}
	else {
		while (currFB != 0) {
			if ((Type::U32)newFH < (Type::U32)currFB) {
				if (currFB->pFreePrev == 0) {
					this->pHeap->pFreeHead = newFH;
					newFH->pFreeNext = currFB;
					newFH->pFreePrev = 0;
					currFB->pFreePrev = newFH;
				}
				else {
					currFB->pFreePrev->pFreeNext = newFH;
					newFH->pFreePrev = currFB->pFreePrev;
					currFB->pFreePrev = newFH;
					newFH->pFreeNext = currFB;
				}
				break;
			}
			else if (currFB->pFreeNext == 0) {
				currFB->pFreeNext = newFH;
				newFH->pFreePrev = currFB;
				newFH->pFreeNext = 0;
				break;
			}
			currFB = currFB->pFreeNext;
		}
	}

}
void Mem::Free(void * const data)
{
	boolean combined = false;
	boolean cbelow = false;
	//move up first before freeing
	UsedHdr *tmp = (UsedHdr*)data - 1; //points at used header
	if (this->pHeap->pUsedHead == tmp) {
		if (this->pHeap->pUsedHead->pUsedNext != 0) {
			UsedHdr *prev = tmp->pUsedNext; //cuz its backward
			this->pHeap->pUsedHead = prev;
			this->pHeap->pUsedHead->pUsedPrev = 0;
		}
		else
			this->pHeap->pUsedHead = 0;
	}
	else{
		if (tmp->pUsedPrev != 0)
			tmp->pUsedPrev->pUsedNext = tmp->pUsedNext;
		if (tmp->pUsedNext != 0)
			tmp->pUsedNext->pUsedPrev = tmp->pUsedPrev;
	}
	

	unsigned int sizeOfToBeFreed = tmp->mBlockSize; //block size
	this->pHeap->mStats.currFreeMem += sizeOfToBeFreed;
	this->pHeap->mStats.currNumFreeBlocks++;
	this->pHeap->mStats.currNumUsedBlocks--;
	this->pHeap->mStats.currUsedMem -= sizeOfToBeFreed;

	FreeHdr *newFH = new((FreeHdr*)tmp)FreeHdr(*tmp);

	FreeHdr *r = (FreeHdr*)((Type::U32)data + sizeOfToBeFreed);
	if ((Type::U32)r != (Type::U32)this->pHeap->mStats.heapBottomAddr) {
		if (r->mBlockType == (Type::U8)BlockType::FREE) {
			newFH = combineBelow(newFH, r);
			combined = true;
			cbelow = true;
			this->pHeap->mStats.currFreeMem += sizeof(FreeHdr);
			this->pHeap->mStats.currNumFreeBlocks--;
		}
	}
	if (newFH->mAboveBlockFree == true) {
		//check above  //pSecret->free = (FreeHdr*)secret;
		Type::U32 hdrStart = (Type::U32)newFH;
		Type::U32 *pSecret = (Type::U32*)(hdrStart - 4);
		FreeHdr *above = (FreeHdr*)*pSecret;
		//if (above->mBlockType == (Type::U8)BlockType::FREE) {
			newFH = combineAbove(newFH, above, cbelow);
			combined = true;
			this->pHeap->mStats.currFreeMem += sizeof(FreeHdr);
			this->pHeap->mStats.currNumFreeBlocks--;
		//}
	}
	if (!combined)
		insert(newFH);

	//flag
	FreeHdr *below = (FreeHdr*)((Type::U32)newFH + sizeof(FreeHdr) + newFH->mBlockSize);
	if ((Type::U32)below < (Type::U32)this->pHeap->mStats.heapBottomAddr)
		below->mAboveBlockFree = true;
	//secret pointer
	//if ((Type::U32)newFH > (Type::U32)this->pHeap->mStats.heapTopAddr) {
		Type::U32 hdrStart = (Type::U32)newFH;
		Type::U32 hdrEnd = hdrStart + sizeof(FreeHdr);
		Type::U32 blkEnd = hdrEnd + newFH->mBlockSize;
		Type::U32 secret = (Type::U32)newFH;
		Type::U32 *sPointer = (Type::U32*)(blkEnd - 4);
		*sPointer = secret;
	//}
	STUB_PLEASE_REPLACE(data);	
}


void Mem::Dump()
{

	fprintf(FileIO::GetHandle(),"\n------- DUMP -------------\n\n");

	fprintf(FileIO::GetHandle(), "heapStart: 0x%p     \n", this->pHeap );
	fprintf(FileIO::GetHandle(), "  heapEnd: 0x%p   \n\n", this->pHeap->mStats.heapBottomAddr );
	fprintf(FileIO::GetHandle(), "pUsedHead: 0x%p     \n", this->pHeap->pUsedHead );
	fprintf(FileIO::GetHandle(), "pFreeHead: 0x%p     \n", this->pHeap->pFreeHead );
	fprintf(FileIO::GetHandle(), " pNextFit: 0x%p   \n\n", this->pHeap->pNextFit);

	fprintf(FileIO::GetHandle(),"Heap Hdr   s: %p  e: %p                            size: 0x%x \n",(void *)((Type::U32)this->pHeap->mStats.heapTopAddr-sizeof(Heap)), this->pHeap->mStats.heapTopAddr, sizeof(Heap) );

	Type::U32 p = (Type::U32)pHeap->mStats.heapTopAddr;

	char *type;
	char *typeHdr;

	while( p < (Type::U32)pHeap->mStats.heapBottomAddr )
	{
		UsedHdr *used = (UsedHdr *)p;
		if( used->mBlockType == (Type::U8)BlockType::USED )
		{
			typeHdr = "USED HDR ";
			type    = "USED     ";
		}
		else
		{
			typeHdr = "FREE HDR ";
			type    = "FREE     ";
		}

		Type::U32 hdrStart = (Type::U32)used;
		Type::U32 hdrEnd   = (Type::U32)used + sizeof(UsedHdr);
		fprintf(FileIO::GetHandle(),"%s  s: %p  e: %p  p: %p  n: %p  size: 0x%x    AF: %d \n",typeHdr, (void *)hdrStart, (void *)hdrEnd, used->pUsedPrev, used->pUsedNext, sizeof(UsedHdr), used->mAboveBlockFree );
		Type::U32 blkStart = hdrEnd;
		Type::U32 blkEnd   = blkStart + used->mBlockSize; 
		fprintf(FileIO::GetHandle(),"%s  s: %p  e: %p                            size: 0x%x \n",type, (void *)blkStart, (void *)blkEnd, used->mBlockSize );

		p = blkEnd;
	
	}
}

// ---  End of File ---------------
