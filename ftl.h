#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512
#define PAGES_PER_BLOCK 256
#define PAGE_SIZE 4096
//#define BLOCK_SIZE 524288
#define BLOCK_SIZE 1048576

#define getBlockNo(ppn) (ppn/PAGES_PER_BLOCK)
#define getPageNo(ppn) (ppn%PAGES_PER_BLOCK)

#define MAX_STREAM 4

long long LOGICAL_FLASH_SIZE ;
long long OP_REGION ;
long long LOGICAL_PAGE ;
long long FLASH_SIZE;

long long BLOCKS_PER_FLASH;
long long PAGES_PER_FLASH;

typedef struct _L2P{
	int ppn;
}L2P;

typedef struct _P2L{
	int lpn;
	int valid;
}P2L;

typedef struct _blockMap{
	int invalidCnt;
	int nextBlk;
	int eraseCount;
	int streamID;
}BlockMap;

/* verify structure is for just veriftying if the ftl works well
 * - FTL works well without this structure
 */
typedef struct _Verify{
	int maxInvalidity;
}Verify; 

typedef struct _FreeMeta{
	int freeBlock;
	int freeHead;
	int freeTail;
}FreeMeta;

typedef struct _UBlock{
	int curBlock;
	int curPage;
}UBlock;

typedef struct _GC{
	long long copyback;
	long long gcCnt;
}GC;

typedef struct _COUNT {
	long long bef_write;
	struct _GC bef_block;
	long long read;
	long long write;
	struct _GC block;
}COUNT;

int streamNum;

COUNT stat;
COUNT streamStat[MAX_STREAM];
FreeMeta freeMeta;
L2P* logicalMap;
P2L* physicalMap;
BlockMap * bMap;
UBlock *updateBlock;
Verify verify;

void printCount();

void M_init();
void M_close();
int M_GC();
void M_read(int lpn);
int M_write(int lpn, int streamID);
