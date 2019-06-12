#include "ftl.h"

int getFreeBlock() {
	int index;

	if (freeMeta.freeBlock == 0){
		printf("[ERROR] (%s, %d) no free block'\n", __func__, __LINE__);
		return -1;
	}
	index = freeMeta.freeHead ;
	freeMeta.freeHead = bMap[index].nextBlk;
	freeMeta.freeBlock --; 

	bMap[index].invalidCnt = 0;

	return index; 
}

void putFreeBlock(int index) {

	bMap[freeMeta.freeTail].nextBlk = index; // needless?

	freeMeta.freeTail = index;
	bMap[freeMeta.freeTail].nextBlk = -1;
	
	bMap[index].invalidCnt = -1;
	
	if(freeMeta.freeHead == -1){
		freeMeta.freeHead = index;
		if(freeMeta.freeHead != freeMeta.freeTail) {
			printf("[ERROR] (%s, %d) free block list errorr", __func__, __LINE__);
		}
	}
	
	freeMeta.freeBlock ++;
}

// greedy victim selection algorithm
int victim_select_greedy() {

	long long i;
	int max, res, secondMax;
	
	res = 0;
	max = bMap[0].invalidCnt;
	secondMax = max;

	// select most invalidated block
	for (i=1 ; i<BLOCKS_PER_FLASH ; i++) {
		if (bMap[i].invalidCnt > max) {
			max = bMap[i].invalidCnt;
			res = i;
		}
	}

	verify.maxInvalidity = max;
	if (max > PAGES_PER_BLOCK){
		printf("[ERROR] (%s, %d) : max invalidirt too high\n", __func__, __LINE__);
		printCount();
	}

	return res;
}

// this function is for searching free block from free block list
// if there is free block less than 1, perform GC
int search_free_block() {

	int index;
	
	if (freeMeta.freeBlock <= 1) {
		// allocate update block
		index = getFreeBlock();
		
		if(index == -1){
			printf("[ERROR] (%s, %d)\n", __func__, __LINE__);
			return -2;
		} else {
			updateBlock[0].curBlock = index;
			updateBlock[0].curPage = 0;
		}

		index = M_GC();
		return -1;
	} 

	index = getFreeBlock();
	if (index == -1 ) {
		printf("[ERROR] (%s, %d)\n", __func__, __LINE__); 		
	}

	updateBlock[0].curBlock = index;
	updateBlock[0].curPage = 0;
	
	if(freeMeta.freeHead == -1) {
		printf("[ERROR] (%s, %d)\n", __func__, __LINE__);
		return -2;
	}

	return index;
}

void initStat() {
	stat.read = 0;
	stat.write = 0;
	stat.block.copyback = 0;
	stat.block.gcCnt = 0;
	stat.bef_write = 0;
	stat.bef_block.copyback = 0;
	stat.bef_block.gcCnt = 0;
}

void M_close() {
	free(updateBlock);
	free(logicalMap);
	free(physicalMap);
	free(bMap);
}

void M_init() {
	long long i;

	initStat();
	
	freeMeta.freeBlock = BLOCKS_PER_FLASH-1;
	freeMeta.freeHead = 1;
	freeMeta.freeTail = BLOCKS_PER_FLASH -1;

	// verification code
	verify.maxInvalidity = 0;

	logicalMap = (L2P*) malloc(sizeof(L2P) * LOGICAL_PAGE);
	physicalMap = (P2L*) malloc(sizeof(P2L) * PAGES_PER_FLASH);
	bMap = (BlockMap*) malloc(sizeof(BlockMap) * BLOCKS_PER_FLASH);

	updateBlock = (UBlock*) malloc (sizeof(UBlock) * 1);
		
	updateBlock[0].curBlock = 0;
	updateBlock[0].curPage = 0;


	for(i=0 ; i<PAGES_PER_FLASH ; i++) {
		if(i < LOGICAL_PAGE){
			logicalMap[i].ppn = -1;
		}
		if(i < BLOCKS_PER_FLASH) {
			bMap[i].invalidCnt = -1;
			bMap[i].nextBlk = i+1; 
			bMap[i].eraseCount =0 ;
		}
		physicalMap[i].lpn = -1;
		physicalMap[i].valid = 0;
	}
	bMap[BLOCKS_PER_FLASH-1].nextBlk = -1;

	bMap[0].invalidCnt = 0;
}


// this function is for GC
int M_GC () {
	int victimBlock = 0, lpn, newPpn, i, index;
	long long index_start, cpBack;


	victimBlock = victim_select_greedy();
	
	// update statistic
	bMap[victimBlock].eraseCount++;
	stat.block.gcCnt ++ ;

	// Copyback if there are valid pages
	index_start = victimBlock * PAGES_PER_BLOCK;
	cpBack = 0;
	for (i = 0 ; i < PAGES_PER_BLOCK  ; i ++ ) {
		if (physicalMap[index_start + i].valid == 1) {
			// GC taget block, page (not separate with write block)
			if (updateBlock[0].curPage >= PAGES_PER_BLOCK) {
				printf("[ERROR] (%s, %d)\n", __func__, __LINE__);
			}

			lpn = physicalMap[index_start + i].lpn ;
			
			// clean olddata
			physicalMap[index_start+i].lpn = -1;
			physicalMap[index_start+i].valid = 0;

			// copyback
			newPpn = updateBlock[0].curPage + updateBlock[0].curBlock * PAGES_PER_BLOCK;

			logicalMap[lpn].ppn = newPpn;
			physicalMap[newPpn].lpn = lpn;
			physicalMap[newPpn].valid = 1;
		
			// update statistic	
			cpBack ++;
			updateBlock[0].curPage ++;
		}
	}

	// update statistic
	stat.block.copyback += cpBack;

	// verify
	if ((PAGES_PER_BLOCK-verify.maxInvalidity) != cpBack){		
		printf("[ERROR] %s - Invalidity not match (%d) (%lld) (%d)\n", __func__, (PAGES_PER_BLOCK-verify.maxInvalidity), cpBack, bMap[victimBlock].eraseCount);
	} 

	// free Block
	putFreeBlock(victimBlock);

	return victimBlock;
}

void M_read (int lpn) {
	stat.read ++;
}


// this function is for write operation
int M_write(int lpn) {

	int oldPpn, newPpn, freeIndex;
;
	stat.write ++;

	if (logicalMap[lpn].ppn != -1 ) {
		// invalidate Old data
		oldPpn = logicalMap[lpn].ppn;
		physicalMap[oldPpn].lpn = -1;
		physicalMap[oldPpn].valid = 0;

		bMap[getBlockNo(oldPpn)].invalidCnt ++;
	}

	// Write New data
	// - where to write data?! --> curBlock, curPage

	// if update block is full, get new free block to write 
	if(updateBlock[0].curPage == PAGES_PER_BLOCK){
		//get new free block
		freeIndex = search_free_block();
		
		if (freeIndex == -2){
			printf("[ERROR] (%s %d) search free block failed", __func__, __LINE__);
			return -2;
		}
		// if freeIndex == -1 - GC is done so that new updateBlock is already allocated in GC step.
	}

	// write data to new physical address
	newPpn = updateBlock[0].curPage + updateBlock[0].curBlock*PAGES_PER_BLOCK;

	logicalMap[lpn].ppn = newPpn;
	physicalMap[newPpn].lpn = lpn;
	physicalMap[newPpn].valid = 1;

	updateBlock[0].curPage ++;
	return 1;
}
