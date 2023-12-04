#include "cache.h"

cache::cache()
{
	for (int i = 0; i < L1_CACHE_SETS; i++)
		L1[i].valid = false;
	for (int i = 0; i < L2_CACHE_SETS; i++)
		for (int j = 0; j < L2_CACHE_WAYS; j++)
			L2[i][j].valid = false;

	// Do the same for Victim Cache ...

	this->myStat.missL1 = 0;
	this->myStat.missL2 = 0;
	this->myStat.accL1 = 0;
	this->myStat.accL2 = 0;

	// Add stat for Victim cache ...
}
void cache::controller(bool MemR, bool MemW, int *data, int adr, int *myMem)
{
	// add your code here
	// Store Algorithm
	if (MemW)
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
	// Store in L1
	// 32-bit address extract tag, index, offset
	const int offsetSize = 2;
	const int indexSize = 4;
	addressInfo info = extractAddressInfo(adr, offsetSize, indexSize);
}

int cache::load(int *data, int adr, int *myMem)
{
	// 32-bit address extract tag, index, offset
	const int offsetSize = 2;
	const int indexSize = 4;

	addressInfo info = extractAddressInfo(adr, offsetSize, indexSize);
	cout << "Tag " << info.tag << endl;
	cout << "Index " << info.index << endl;
	cout << "Offset " << info.offset << endl;
	// Check L1 Cache
	// Index into correct spot in L1`
	cacheBlock L1Block = L1[info.index];
	myStat.accL1++;
	if (L1search(info))
	{
		cout << "L1 Hit" << endl;
		return L1Block.data;
	}
	else
	{
		cout << "L1 Miss" << endl;
		myStat.missL1++;
	}

	// Check Victim Cache
	int victimIndex = victimSearch(info);
	// victim cache hit
	myStat.accVic++;
	if (victimIndex != -1)
	{
		cout << "Victim Hit" << endl;
		// Swap L1 and Victim Line
		cacheBlock evictedL1Block = L1Replace(info, victimCache[victimIndex]);
		if (evictedL1Block.valid)
		{
			// Puts evictedL1Block into victim cache
			evictVictim(victimIndex, evictedL1Block);
			return L1[info.index].data;
		}
	}
	// Victim Miss
	else
	{
		cout << "Victim Miss" << endl;
		myStat.missVic++;
	}

	// Search L2
	myStat.accL2++;
	int l2index = L2search(info);
	// L2 Hit
	if (l2index != -1)
	{
		cout << "L2 Hit" << endl;
		cacheBlock hitBlock = L2[info.index][l2index];
		// Move block to L1
		cacheBlock oldL1Block = L1Replace(info, hitBlock);
		oldL1Block.lru_position = 0;
		if (!oldL1Block.valid)
			return hitBlock.data;

		// Move from L1 to Victim
		addressInfo oldL1Info = extractAddressInfo(oldL1Block.data, offsetSize, indexSize);
		// Find oldest block in victim and evict to L2
		int lru = victimFindLRU();
		cacheBlock evictedVictimBlock = evictVictim(lru, oldL1Block);
		if (!evictedVictimBlock.valid)
			return hitBlock.data;

		// Move evicted victim block to L2
		addressInfo evictedVictimInfo = extractAddressInfo(evictedVictimBlock.data, offsetSize, indexSize);
		cacheBlock evictedL2Block = L2Replace(evictedVictimInfo, evictedVictimBlock);
	}
	else
	{
		cout << "L2 Miss" << endl;
		myStat.missL2++;

		// Search main memory
		cacheBlock mainMemoryBlock = cacheBlock();
		mainMemoryBlock.data = myMem[adr];
		addressInfo mainMemoryInfo = extractAddressInfo(mainMemoryBlock.data, offsetSize, indexSize);
		mainMemoryBlock.valid = true;
		mainMemoryBlock.tag = mainMemoryInfo.tag;
		// Move main memory block to L1
		cacheBlock oldL1Block = L1Replace(info, mainMemoryBlock);
		oldL1Block.lru_position = 0;
		if (!oldL1Block.valid)
			return myMem[adr];

		// Move from L1 to Victim
		addressInfo oldL1Info = extractAddressInfo(oldL1Block.data, offsetSize, indexSize);
		// Find oldest block in victim and evict to L2
		int lru = victimFindLRU();
		cacheBlock evictedVictimBlock = evictVictim(lru, oldL1Block);
		if (!evictedVictimBlock.valid)
			return myMem[adr];

		// Move evicted victim block to L2
		addressInfo evictedVictimInfo = extractAddressInfo(evictedVictimBlock.data, offsetSize, indexSize);
		cacheBlock evictedL2Block = L2Replace(evictedVictimInfo, evictedVictimBlock);
		return myMem[adr];
	}
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
	for (int i = 0; i < VICTIM_SIZE; i++)
	{
		if (victimCache[i].valid && victimCache[i].tag == info.tag)
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
		if (victimCache[i].valid && victimCache[i].lru_position < pivot)
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
	for (int i = 0; i < L2_CACHE_WAYS; i++)
	{
		if (L2[info.index][i].valid && L2[info.index][i].lru_position == L2_CACHE_WAYS - 1)
		{
			cacheBlock oldBlock = L2[info.index][i];
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