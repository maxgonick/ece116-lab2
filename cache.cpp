#include "cache.h"

cache::cache()
{
	for (int i = 0; i < L1_CACHE_SETS; i++)
		L1[i].valid = false;
	for (int i = 0; i < L2_CACHE_SETS; i++)
		for (int j = 0; j < L2_CACHE_WAYS; j++)
			L2[i][j].valid = false;

	this->myStat.missL1 = 0;
	this->myStat.missL2 = 0;
	this->myStat.accL1 = 0;
	this->myStat.accL2 = 0;
}
void cache::controller(bool MemR, bool MemW, int *data, int adr, int *myMem)
{

	// Store Algorithm
	if (MemR)
	{
		load(data, adr, myMem);
	}
	// Load
	else
	{
		store(data, adr, myMem);
	}
}

void cache::store(int *data, int adr, int *myMem)
{
	// 32-bit address extract tag, index, offset
	const int offsetSize = 2;
	const int indexSize = 4;
	addressInfo info = extractAddressInfo(adr, offsetSize, indexSize);
	cacheBlock storeBlock;
	storeBlock.data = *data;
	storeBlock.tag = info.tag;
	storeBlock.valid = 1;
	storeBlock.lru_position = 0;

	// Search L1

	// L1 Hit
	if (L1search(info))
	{
		// Update Data in L1
		L1[info.index] = storeBlock;
		// Update Data in Main Memory
		myMem[adr] = *data;
		return;
	}

	// Search Victim
	int vicIndex = victimSearch(info);
	// Victim Hit
	if (vicIndex != -1)
	{
		// Re-calculate LRU positions
		int pivot = victimCache[vicIndex].lru_position;
		for (int i = 0; i < VICTIM_SIZE; i++)
		{
			if (victimCache[i].valid && victimCache[i].lru_position < pivot)
			{
				victimCache[i].lru_position++;
			}
		}
		victimCache[vicIndex] = storeBlock;
		myMem[adr] = *data;
		return;
	}

	// Search L2
	int L2Index = L2search(info);
	if (L2Index != -1)
	{
		// Re-calculate LRU positions
		int pivot = L2[info.index][L2Index].lru_position;
		for (int i = 0; i < L2_CACHE_WAYS; i++)
		{
			if (L2[info.index][i].valid && L2[info.index][i].lru_position < pivot)
			{
				L2[info.index][i].lru_position++;
			}
		}
		L2[info.index][L2Index] = storeBlock;
		myMem[adr] = *data;
		return;
	}
	// Always update main memory!
	myMem[adr] = *data;
	return;
}

int cache::load(int *data, int adr, int *myMem)
{
	// 32-bit address extract tag, index, offset
	const int offsetSize = 2;
	const int indexSize = 4;

	addressInfo info = extractAddressInfo(adr, offsetSize, indexSize);

	// Check L1 Cache
	// Index into correct spot in L1
	cacheBlock L1Block = L1[info.index];
	myStat.accL1++;
	// L1 Hit
	if (L1search(info))
	{
		return L1Block.data;
	}
	else
	{
		myStat.missL1++;
	}

	// Check Victim Cache
	int victimIndex = victimSearch(info);
	// victim cache hit
	myStat.accVic++;
	if (victimIndex != -1)
	{
		//  Swap L1 and Victim Line
		cacheBlock newL1Block;
		newL1Block.adr = victimCache[victimIndex].adr;
		newL1Block.tag = extractAddressInfo(newL1Block.adr, offsetSize, indexSize).tag;
		newL1Block.data = victimCache[victimIndex].data;
		newL1Block.valid = 1;
		cacheBlock evictedL1Block = L1Replace(info, newL1Block);
		if (evictedL1Block.valid)
		{
			addressInfo adrtemp;
			adrtemp = extractAddressInfo(evictedL1Block.adr, offsetSize, indexSize);
			//  Puts evictedL1Block into victim cache with L1/L2 tag formatting
			evictedL1Block.tag = (evictedL1Block.tag << 4) | extractAddressInfo(newL1Block.adr, offsetSize, indexSize).index;
			evictVictim(victimIndex, evictedL1Block);
			return L1[info.index].data;
		}
	}
	// Victim Miss
	else
	{
		myStat.missVic++;
	}

	// Search L2
	myStat.accL2++;
	int l2index = L2search(info);
	// L2 Hit
	if (l2index != -1)
	{
		cacheBlock hitBlock = L2[info.index][l2index];
		// Move block to L1
		cacheBlock oldL1Block = L1Replace(info, hitBlock);
		oldL1Block.lru_position = 0;
		if (!oldL1Block.valid)
			return hitBlock.data;

		// Move from L1 to Victim
		addressInfo oldL1Info = extractAddressInfo(oldL1Block.adr, offsetSize, indexSize);
		cacheBlock newVictimBlock;

		// Find oldest block in victim and evict to L2
		int lru = victimFindLRU();

		newVictimBlock.adr = oldL1Block.adr;
		// Calculate Victim Tag
		newVictimBlock.tag = (oldL1Block.tag << 4) | oldL1Info.index;
		newVictimBlock.data = oldL1Block.data;
		newVictimBlock.valid = 1;

		cacheBlock evictedVictimBlock = evictVictim(lru, newVictimBlock);
		if (!evictedVictimBlock.valid)
			return hitBlock.data;

		// Move evicted victim block to L2
		addressInfo evictedVictimInfo = extractAddressInfo(evictedVictimBlock.adr, offsetSize, indexSize);
		evictedVictimBlock.tag = evictedVictimInfo.tag;
		cacheBlock evictedL2Block = L2Replace(evictedVictimInfo, evictedVictimBlock);
	}
	else
	{
		myStat.missL2++;

		// Search main memory and create cacheBlock from data
		cacheBlock mainMemoryBlock = cacheBlock();
		mainMemoryBlock.data = myMem[adr];
		mainMemoryBlock.valid = true;
		mainMemoryBlock.tag = info.tag;
		mainMemoryBlock.adr = adr;
		// Move main memory block to L1
		cacheBlock oldL1Block = L1Replace(info, mainMemoryBlock);
		oldL1Block.lru_position = 0;
		// See if we need to evict a valid L1 block to victim
		if (!oldL1Block.valid)
			return myMem[adr];

		// Move from L1 to Victim
		addressInfo oldL1Info = extractAddressInfo(oldL1Block.adr, offsetSize, indexSize);
		cacheBlock newVictimBlock;

		// Find oldest block in victim and evict to L2
		int lru = victimFindLRU();
		newVictimBlock.adr = oldL1Block.adr;
		newVictimBlock.tag = (oldL1Block.tag << 4) | oldL1Info.index;
		newVictimBlock.data = oldL1Block.data;
		newVictimBlock.valid = 1;

		cacheBlock evictedVictimBlock = evictVictim(lru, newVictimBlock);
		// cout << "Moving Block to victim with index " << oldL1Info.index << "and tag " << oldL1Block.tag;
		if (!evictedVictimBlock.valid)
			return myMem[adr];

		// Move evicted victim block to L2
		addressInfo evictedVictimInfo = extractAddressInfo(evictedVictimBlock.adr, offsetSize, indexSize);
		evictedVictimBlock.tag = evictedVictimInfo.tag;
		cacheBlock evictedL2Block = L2Replace(evictedVictimInfo, evictedVictimBlock);
		return myMem[adr];
	}
	return myMem[adr];
}

addressInfo cache::extractAddressInfo(int adr, int offsetSize, int indexSize)
{
	addressInfo info;

	// Extract masks
	uint32_t offsetMask = (1u << offsetSize) - 1;
	uint32_t indexMask = ((1u << indexSize) - 1) << offsetSize;
	uint32_t tagMask = ~((1u << (offsetSize + indexSize)) - 1);

	uint32_t offset = adr & offsetMask;
	uint32_t index = (adr & indexMask) >> offsetSize;
	uint32_t tag = (adr & tagMask) >> (offsetSize + indexSize);

	// Set info
	info.tag = tag;
	info.index = index;
	info.offset = offset;

	return info;
}

Stat cache::returnStat()
{
	return myStat;
}

cacheBlock cache::L1update(int *data, addressInfo info)
{
	cacheBlock oldBlock = L1[info.index];
	cacheBlock L1Block = L1[info.index];
	L1Block.data = *data;
	L1Block.tag = info.tag;
	L1Block.valid = true;
	L1[info.index] = L1Block;
	return oldBlock;
}

bool cache::L1search(addressInfo info)
{

	cacheBlock L1Block = L1[info.index];
	if (L1Block.valid && L1Block.tag == info.tag)
	{
		return true;
	}
	return false;
}

int cache::victimSearch(addressInfo info)
{
	addressInfo vicInfo = info;
	vicInfo.tag = (info.tag << 4) | info.index;
	for (int i = 0; i < VICTIM_SIZE; i++)
	{
		if (victimCache[i].valid && victimCache[i].tag == vicInfo.tag)
		{
			return i;
		}
	}
	return -1;
}

int cache::L2search(addressInfo info)
{
	for (int i = 0; i < L2_CACHE_WAYS; i++)
	{
		if (L2[info.index][i].valid && L2[info.index][i].tag == info.tag)
		{
			return i;
		}
	}
	return -1;
}

cacheBlock cache::evictVictim(int victimIndex, cacheBlock newBlock)
{

	cacheBlock oldVictimBlock = victimCache[victimIndex];
	int pivot = victimCache[victimIndex].lru_position;
	for (int i = 0; i < VICTIM_SIZE; i++)
	{
		if (!victimCache[victimIndex].valid && victimCache[i].valid)
		{
			victimCache[i].lru_position++;
		}
		else if (victimCache[i].valid && victimCache[i].lru_position < pivot)
		{
			victimCache[i].lru_position++;
		}
	}
	victimCache[victimIndex] = newBlock;
	return oldVictimBlock;
}

// Replace L1 Block and Evict old Block
cacheBlock cache::L1Replace(addressInfo info, cacheBlock newBlock)
{
	cacheBlock oldBlock = L1[info.index];
	L1[info.index] = newBlock;
	return oldBlock;
}

cacheBlock cache::L2Replace(addressInfo info, cacheBlock newBlock)
{
	int max = -1;
	for (int i = 0; i < L2_CACHE_WAYS; i++)
	{
		if (L2[info.index][i].valid && L2[info.index][i].lru_position > max)
		{
			max = L2[info.index][i].lru_position;
		}
		if (!L2[info.index][i].valid)
		{
			max = -1;
			break;
		}
	}
	// Puts new block into victim cache and re-calculates LRU positions
	for (int i = 0; i < L2_CACHE_WAYS; i++)
	{
		if ((L2[info.index][i].lru_position >= max && max != -1) || (max == -1 && !L2[info.index][i].valid))
		{
			cacheBlock oldBlock = L2[info.index][i];
			newBlock.valid = 1;
			L2[info.index][i] = newBlock;
			L2[info.index][i].lru_position = 0;
			for (int j = 0; j < L2_CACHE_WAYS; j++)
			{
				if (L2[info.index][j].valid && L2[info.index][j].lru_position < oldBlock.lru_position)
				{
					L2[info.index][j].lru_position++;
				}
			}
			return oldBlock;
		}
	}
	cacheBlock nonValid = newBlock;
	nonValid.valid = false;
	return nonValid;
}

int cache::victimFindLRU()
{
	int max = -1;
	int maxIndex = -1;
	for (int i = 0; i < VICTIM_SIZE; i++)
	{
		if (victimCache[i].lru_position > max)
		{
			max = victimCache[i].lru_position;
			maxIndex = i;
		}
		if (!victimCache[i].valid)
		{
			return i;
		}
	}
	return maxIndex;
}
