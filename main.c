#include "ftl.h"
#include <unistd.h>

char logFile[100];
char statFile[100];

long KB = 1024;
long MB = 1024 * 1024;
long GB = 1024 * 1024 * 1024;


int Compare(const void * p1, const void *p2) {
	int a, b;

	a = *(int*) p1;
	b = *(int*) p2;

	if (a > b)
		return -1;
	else if (a < b)
		return 1;
	else 
		return 0;
}

int printBlkStat () {
	FILE * statf;
	int i ;
	int resEr[BLOCKS_PER_FLASH], resVa[BLOCKS_PER_FLASH];


	if ((statf = fopen(statFile, "w+")) < 0){
		printf("[ERROR] (%s, %d) file open fail\n", __func__, __LINE__);	
		return -1;
	}


	// validity
	for ( i=0 ; i < BLOCKS_PER_FLASH ; i++ )
		resVa[i] = PAGES_PER_BLOCK - bMap[i].invalidCnt;

	// erase count
	for (i=0 ; i<BLOCKS_PER_FLASH ; i ++)
		resEr[i] = bMap[i].eraseCount;

	qsort(resVa, BLOCKS_PER_FLASH, sizeof(int), Compare);
	qsort(resEr, BLOCKS_PER_FLASH, sizeof(int), Compare);


	for (i=0 ; i<BLOCKS_PER_FLASH ; i ++)
		fprintf(statf, "%d\t%d\n", resVa[i], resEr[i]);

	fclose(statf);

	return 0;
}

void printConf() {
	/// print configuration
	printf("[DEBUG] logical size:\t%lld GB\n", LOGICAL_FLASH_SIZE/GB);
	printf("[DEBUG] op region:\t%lld\n", OP_REGION);
	printf("[DEBUG] flash size:\t%lld\n", FLASH_SIZE);
	printf("[DEBUG] logical page:\t%lld\n", LOGICAL_PAGE);
	printf("[DEBUG] flash block:\t%lld\n", BLOCKS_PER_FLASH);
	printf("[DEBUG] flash page:\t%lld\n", PAGES_PER_FLASH);
	printf("[DEBUG] stream Mode:\t%s ", (streamNum==1?"Original":"Multistream"));
	if (streamNum > 1)
		printf(" (%d)\n", streamNum);
	else 
		printf("\n");
}

int initConf(int argc, char* argv[]) {
	int i ;
	int param_opt, op;

	LOGICAL_FLASH_SIZE = 0;
	streamNum = 1;
	op = 10;

	// option get
	while ((param_opt = getopt(argc, argv, "s:f:o:r:m:")) != -1){
		switch(param_opt)
		{
			case 's':
				sscanf(optarg,"%lld", &LOGICAL_FLASH_SIZE);
				LOGICAL_FLASH_SIZE *= GB;
				break;
			case 'f': 
				strncpy(logFile, optarg, strlen(optarg));
				 break;
			case 'o':
				sscanf(optarg, "%d", &op);
				break;
			case 'r':
				strncpy(statFile, optarg, strlen(optarg));
				break;
			case 'm':
				sscanf(optarg, "%d", &streamNum);
				break;
		}
	}
	if (logFile[0] == 0){
		printf("Please input log file name: -f [logFilename]\n");
		return -1;
	}

	
	if(LOGICAL_FLASH_SIZE == 0 ){
		printf("Please input flash size (opt: -s [x GB])\n");
		return -1;
	}

	LOGICAL_PAGE = LOGICAL_FLASH_SIZE / PAGE_SIZE;


	OP_REGION = LOGICAL_FLASH_SIZE * op / 100; 
	FLASH_SIZE = LOGICAL_FLASH_SIZE + OP_REGION;
	BLOCKS_PER_FLASH = FLASH_SIZE / BLOCK_SIZE;
	PAGES_PER_FLASH = FLASH_SIZE / PAGE_SIZE;

	return 0;
}

void printCount() {
	int i;

	printf("\nread: %lld\t\twrite: %lld\n", stat.read, stat.write);
	printf("gc: %lld\tcopyback: %lld\n\n", stat.block.gcCnt, stat.block.copyback);
	if(stat.write!=0)
		printf("WAF: %lf\n", (double)(stat.write+stat.block.copyback)/stat.write);

	if((stat.write - stat.bef_write) != 0)
		printf("R_WAF: %lf \n\n", (double)((double)(stat.write-stat.bef_write+stat.block.copyback-stat.bef_block.copyback)/(stat.write - stat.bef_write)));
	

	for (i=0 ; i<streamNum ; i++) {
		if(streamStat[i].write != 0)
			printf("stream%d: %lld %lf \t", i, streamStat[i].write, (double)((double)(streamStat[i].write+streamStat[i].block.copyback)/streamStat[i].write));
	}
	printf("\n");

	stat.bef_write = stat.write;
	stat.bef_block.copyback = stat.block.copyback;
	stat.bef_block.gcCnt = stat.block.gcCnt;

}

int trace_parsing (FILE* fp, long long *start_LPN) {
	char str[1024];
	char * ptr;
	long long lpn;

	fgets(str, 1024, fp);

	if (feof(fp))
		return -1;

	if((ptr = strchr(str, 'W')))
	{
		ptr = ptr+2;
		sscanf(ptr, "%lld", &lpn);

		if(lpn*SECTOR_SIZE/PAGE_SIZE < LOGICAL_PAGE)
			*start_LPN = lpn*SECTOR_SIZE/PAGE_SIZE;
		else {
			printf("[ERROR] (%s, %d) lpn range\n", __func__, __LINE__);
			return -1;
		}

		return 1;
	} else 
		return -1;

	return 0;
}

int main (int argc, char* argv[]) {
	FILE * inputFp;
	int offCnt, opCode, op_count;
	long long start_LPN;

	logFile[0] = 0;
	statFile[0] = 0;
	if( initConf(argc, argv)< 0)
		return 0;
	

	if ( (inputFp = fopen(logFile, "r")) == 0 ) {
		printf("Open File Fail \n");
		exit(1);
	}

	printConf();

	M_init();

	op_count = 0;
	while (1) {
		opCode = trace_parsing(inputFp,&start_LPN);

		// opCode : operation
		if (opCode == 1) {
			if(M_write(start_LPN, 0) < 0) {
				printf("[ERROR] (%s, %d) write failed\n", __func__, __LINE__);
				break;
			}
		} else if(opCode == -1)
			break;
		op_count ++;
		if(op_count %2500000 == 0){
			printCount();
		}
	}

	fclose(inputFp);

	printCount();

	if(statFile[0] != 0) {
		printBlkStat();
	}

	M_close();
	return 0;
}
