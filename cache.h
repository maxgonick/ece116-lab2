#include <iostream>
#include <bitset>
#include <stdio.h>
#include <stdlib.h>
#include <string>
using namespace std;

#define L1_CACHE_SETS 16
#define L2_CACHE_SETS 16
#define VICTIM_SIZE 4
#define L2_CACHE_WAYS 8
#define MEM_SIZE 4096
#define BLOCK_SIZE 4 // bytes per block
#define DM 0
#define SA 1

struct addressInfo
{
	uint32_t tag;
	uint32_t index;
	uint32_t offset;
};

struct cacheBlock
{
	int tag;		  // you need to compute offset and index to find the tag.
	int lru_position; // for SA only
	int data;		  // the actual data stored in the cache/memory
	bool valid;
	// add more things here if needed
	int adr;
};

struct Stat
{
	int missL1;
	int missL2;
	int accL1;
	int accL2;
	int accVic;
	int missVic;
	// add more stat if needed. Don't forget to initialize!
};

class cache
{
private:
	cacheBlock L1[L1_CACHE_SETS];				 // 1 set per row.
	cacheBlock L2[L2_CACHE_SETS][L2_CACHE_WAYS]; // x ways per row
	// Add your Victim cache here ...
	cacheBlock victimCache[VICTIM_SIZE];

	Stat myStat;
	// add more things here
public:
	cache();
	void controller(bool MemR, bool MemW, int *data, int adr, int *myMem);
	// add more functions here ...
	void store(int *data, int adr, int *myMem);
	int load(int *data, int adr, int *myMem);
	addressInfo extractAddressInfo(int adr, int offsetSize, int indexSize);
	Stat returnStat();
	cacheBlock L1update(int *data, addressInfo info);
	bool L1search(addressInfo info);
	int victimSearch(addressInfo info);
	int L2search(addressInfo info);
	cacheBlock evictVictim(int victimIndex, cacheBlock newBlock);
	cacheBlock L1Replace(addressInfo info, cacheBlock newBlock);
	cacheBlock L2Replace(addressInfo info, cacheBlock newBlock);
	int victimFindLRU();
	void L1display();
	void L2display();
	void VICdisplay();
};
