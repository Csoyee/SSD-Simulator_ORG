#include "ftl.h"

/*
 * TAG 
 * > @FB: free block management functions (getFreeblock, putFreeBlock, searchFreeBlock)
 * > @VIC: victim selection functions
 * > @GC: GC function (there are 2 - GC for original pm ftl, GC for multi-stream)
 */


// (@FB) free block management function ///
int getFreeBlock(int streamID) {
	int index;

	if (freeMeta.freeBlock == 0){
		printf("[ERROR] (%s, %d) no free block'\n", __func__, __LINE__);
		return -1;
	}
	index = freeMeta.freeHead ;
	freeMeta.freeHead = bMap[index].nextBlk;
	freeMeta.freeBlock --; 

	bMap[index].invalidCnt = 0;
	bMap[index].streamID = streamID;

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
			printf("[ERROR] (%s, %d) free block list error", __func__, __LINE__);
		}
	}
	
	freeMeta.freeBlock ++;
}

// this function is for searching free block from free block list
// if there is free block less than 1, perform GC
int searchFreeBlock(int streamID) {

	int index;
	
	if (streamNum == 1) {
		if (freeMeta.freeBlock <= 1) {
			// allocate update block
			index = getFreeBlock(0);
		
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

		index = getFreeBlock(0);
		if (index == -1 ) {
			printf("[ERROR] (%s, %d)\n", __func__, __LINE__); 		
		}

		updateBlock[0].curBlock = index;
		updateBlock[0].curPage = 0;
		
		if(freeMeta.freeHead == -1) {
			printf("[ERROR] (%s, %d)\n", __func__, __LINE__);
			return -2;
		}
	} else {
		while (freeMeta.freeBlock <= 1) {
			index = M_GC_stream();
			if(index < 0)	return -2;
		}

		index = getFreeBlock(streamID);
		if(index == -1)
			printf("[ERROR] (%s, %d)\n", __func__, __LINE__);

		updateBlock[streamID].curBlock = index;
		updateBlock[streamID].curPage = 0;

		if (freeMeta.freeHead == -1) {
			printf("[ERROR] (%s, %d)\n", __func__, __LINE__);
			return -2;
		}
	}

	return index;
}

////////
// (@VIC) greedy victim selection algorithm
int victim_select_greedy() {

	long long i;
	int max, res, secondMax, flag;
	
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

	for(i=0 ; i<streamNum ; i++)
		if(res == updateBlock[i].curBlock) {
			printf("[NOTE] (%s, %d) : select update block as a victim (%d %lld)\n", __func__, __LINE__,res, i);
		}

	verify.maxInvalidity = max;
	if (max > PAGES_PER_BLOCK){
		printf("[ERROR] (%s, %d) : max invalidity too high\n", __func__, __LINE__);
		printCount();
	}

	return res;
}
/////

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
	
	freeMeta.freeBlock = BLOCKS_PER_FLASH;
	freeMeta.freeTail = BLOCKS_PER_FLASH -1;

	// verification code
	verify.maxInvalidity = 0;

	logicalMap = (L2P*) malloc(sizeof(L2P) * LOGICAL_PAGE);
	physicalMap = (P2L*) malloc(sizeof(P2L) * PAGES_PER_FLASH);
	bMap = (BlockMap*) malloc(sizeof(BlockMap) * BLOCKS_PER_FLASH);

	updateBlock = (UBlock*) malloc (sizeof(UBlock) * streamNum); // update block 개수가 stream 개수만큼 존재
		
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

	/*
	for (i=0 ; i<streamNum ; i++) {
		updateBlock[i].curBlock = i;
		updateBlock[i].curPage = 0;
		bMap[i].invalidCnt = 0;
		bMap[i].streamID = i;
		freeMeta.freeBlock--;
	}
	freeMeta.freeHead = streamNum;
	*/
	for (i=0 ; i<streamNum ; i++) {
		updateBlock[i].curBlock = -1;
		updateBlock[i].curPage = PAGES_PER_BLOCK;
	}
	freeMeta.freeHead = 0;
}


// (@GC)
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
		printf("[ERROR] (%s, %d) - Invalidity not match (%d) (%lld) (%d)\n", __func__, __LINE__,(PAGES_PER_BLOCK-verify.maxInvalidity), cpBack, bMap[victimBlock].eraseCount);
	} 

	// free Block
	putFreeBlock(victimBlock);

	return victimBlock;
}

// this function is for GC
int M_GC_stream () {
	int victimBlock = 0, lpn, newPpn, i, index, victimStream;
	int index_start, cpBack;


	victimBlock = victim_select_greedy();
	
	// update statistic
	bMap[victimBlock].eraseCount++;
	stat.block.gcCnt ++ ;
	
	victimStream = bMap[victimBlock].streamID;

	// Copyback if there are valid pages
	index_start = victimBlock * PAGES_PER_BLOCK;
	cpBack = 0;
	for (i = 0 ; i < PAGES_PER_BLOCK  ; i ++ ) {
		if (physicalMap[index_start + i].valid == 1) {
			// GC taget block, page (not separate with write block)
			if (updateBlock[victimStream].curPage >= PAGES_PER_BLOCK) {
				index = getFreeBlock(victimStream) ;
				if (index == -1)
					printf("[ERROR] (%s, %d) no free block for GC\n", __func__, __LINE__);
				
				updateBlock[victimStream].curBlock = index;
				updateBlock[victimStream].curPage = 0;
			}

			lpn = physicalMap[index_start + i].lpn ;
			
			// clean olddata
			physicalMap[index_start+i].lpn = -1;
			physicalMap[index_start+i].valid = 0;

			// copyback
			newPpn = updateBlock[victimStream].curPage + updateBlock[victimStream].curBlock * PAGES_PER_BLOCK;

			logicalMap[lpn].ppn = newPpn;
			physicalMap[newPpn].lpn = lpn;
			physicalMap[newPpn].valid = 1;
		
			// update statistic	
			cpBack ++;
			updateBlock[victimStream].curPage ++;
		}
	}

	// update statistic
	stat.block.copyback += cpBack;

	// verify
	if ((PAGES_PER_BLOCK-verify.maxInvalidity) != cpBack){		
		printf("[ERROR] (%s, %d) - Invalidity not match (%d/%d) (%d) (%d)\n", __func__, __LINE__, (PAGES_PER_BLOCK-verify.maxInvalidity), cpBack, bMap[victimBlock].eraseCount, victimStream);
		printf("update block: %d %d\n", updateBlock[victimStream].curBlock, victimBlock);
		return -1;
	} 

	// free Block
	putFreeBlock(victimBlock);

	return victimBlock;
}

//////


void M_read (int lpn) {
	stat.read ++;
}


// this function is for write operation
int M_write(int lpn, int streamID) {

	int oldPpn, newPpn, freeIndex;

	
	if (streamID >= streamNum) {
//		printf("[NOTICE] streamID exceeds, change streamID to %d\n", streamNum -1);
		streamID = streamNum -1; 
	}
	

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
	if(updateBlock[streamID].curPage == PAGES_PER_BLOCK){
		//get new free block
		freeIndex = searchFreeBlock(streamID);
		
		if (freeIndex == -2){
			printf("[ERROR] (%s %d) search free block failed (%d)\n", __func__, __LINE__, freeMeta.freeBlock);
			return -2;
		}
		// if freeIndex == -1 - GC is done so that new updateBlock is already allocated in GC step.
	}

	// write data to new physical address
	newPpn = updateBlock[streamID].curPage + updateBlock[streamID].curBlock*PAGES_PER_BLOCK;

	logicalMap[lpn].ppn = newPpn;
	physicalMap[newPpn].lpn = lpn;
	physicalMap[newPpn].valid = 1;

	updateBlock[streamID].curPage ++;
	return 1;
}
