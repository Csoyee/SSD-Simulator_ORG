#include "ftl.h"
#include <unistd.h>

char logFile[100];
char loadFile[100];
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
	int i, j ;
	int resEr[BLOCKS_PER_FLASH], resVa[BLOCKS_PER_FLASH];


	if ((statf = fopen(statFile, "w+")) < 0){
		printf("[ERROR] (%s, %d) file open fail\n", __func__, __LINE__);	
		return -1;
	}


	// validity
	for ( i=0 ; i < BLOCKS_PER_FLASH ; i++ )
		resVa[i] = PAGES_PER_BLOCK - bMap[i].invalidCnt;

	// erase count
	//for (i=0 ; i<BLOCKS_PER_FLASH ; i ++)
	//	resEr[i] = bMap[i].eraseCount;

	qsort(resVa, BLOCKS_PER_FLASH, sizeof(int), Compare);
	//qsort(resEr, BLOCKS_PER_FLASH, sizeof(int), Compare);


	for (i=0 ; i<BLOCKS_PER_FLASH ; i ++)
		fprintf(statf, "%d\n", resVa[i]);

	int addr, befAddr=0, sequential=0, partCnt =0, cnt = 0 ;
	for (i=0 ; i<BLOCKS_PER_FLASH ; i++) {
		befAddr = 0;
		partCnt = 0;
		for(j=0 ; j<PAGES_PER_BLOCK; j++) {
			if(j % 512 == 0)
				fprintf (statf,"---\n");

			fprintf(statf, "%d", physicalMap[i*PAGES_PER_BLOCK + j].lpn);
			addr = physicalMap[i*PAGES_PER_BLOCK+j].lpn;
			if(befAddr +1 == addr) {
				sequential ++;
			} else {
				if(addr != -1){
					cnt++;
					partCnt ++;
					fprintf(statf, "(*)");
				}
			}
			fprintf(statf, "\t");
			befAddr = addr;

			if(j%4 == 3) {
				fprintf(statf, "\n");
			}
		}
		fprintf (statf,"--- (%d / %d)\n\n\n", bMap[i].eraseCount, partCnt);
	}

	fprintf(statf, "Sequentiality: %d, %d, %f\n", sequential, cnt, (float)sequential/cnt);
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
	op = 7;

	// option get
	while ((param_opt = getopt(argc, argv, "s:f:o:r:m:l:")) != -1){
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
			case 'l':
				strncpy(loadFile, optarg, strlen(optarg));
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

int trace_parsing (FILE* fp, long long *start_LPN, long long *length) {
	char str[1024];
	char * ptr, *new;
	long long lpn, len;
	int count;

	fgets(str, 1024, fp);

	if (feof(fp)){
		printf("END!\n");
		return -1;
	}

	// TODO: write length 인지, discard option 인지하는 기능
	if((ptr = strchr(str, 'W')))
	{
		new = strtok(ptr, " ");
		count = 0;
		while(new != NULL ) {
			if(count == 1) {
				sscanf(new, "%lld", &lpn);
			} else if (count == 3) {
				sscanf(new, "%lld", &len);
			}
			new = strtok(NULL, " ");
			count ++;
		}


		if((lpn+len)*SECTOR_SIZE/PAGE_SIZE < LOGICAL_PAGE) {
			*start_LPN = lpn*SECTOR_SIZE/PAGE_SIZE;
			*length = len*SECTOR_SIZE/PAGE_SIZE;
		}
		else {
			printf("[ERROR] (%s, %d) lpn range\n", __func__, __LINE__);
			return -1;
		}

		return 1;
	} else {
		printf("%s\n", str);
		return 0;
	}
	return 0;
}

int trace_parsing2 (FILE* fp, long long *start_LPN, long long *length) {
	char str[1024];
	char * ptr, *new;
	long long lpn, len;
	int count;

	fgets(str, 1024, fp);

	if (feof(fp)){
		printf("END!\n");
		return -1;
	}

	// TODO: write length 인지, discard option 인지하는 기능
	if((ptr = strchr(str, 'W')))
	{
		new = strtok(ptr, " ");
		count = 0;
		while(new != NULL ) {
			if(count == 1) {
				sscanf(new, "%lld", &lpn);
			} else if (count == 2) {
				sscanf(new, "%lld", &len);
			}
			new = strtok(NULL, " ");
			count ++;
		}


		if((lpn+len)*SECTOR_SIZE/PAGE_SIZE < LOGICAL_PAGE) {
			*start_LPN = lpn*SECTOR_SIZE/PAGE_SIZE;
			*length = len*SECTOR_SIZE/PAGE_SIZE;
		}
		else {
			printf("[ERROR] (%s, %d) lpn range\n", __func__, __LINE__);
			return -1;
		}

		return 1;
	} else {
		printf("%s\n", str);
		return 0;
	}
	return 0;
}

int main (int argc, char* argv[]) {
	FILE * inputFp;
	int offCnt, opCode, op_count, i ;
	long long start_LPN, length;

	logFile[0] = 0;
	statFile[0] = 0;
	logFile[0] = 0;
	if( initConf(argc, argv)< 0)
		return 0;
	
	M_init();

	if(loadFile[0]) {
		if ( (inputFp = fopen(loadFile, "r")) == 0 ) {
			printf("Open File Fail \n");
			exit(1);
		}


		printf("Start Loading\n");
		while (1) {
			opCode = trace_parsing2(inputFp,&start_LPN, &length);
	
			// opCode : operation
			if (opCode == 1) {
				for (i=0 ; i< length  ; i++) {
					if(M_write(start_LPN+i, 0) < 0) {
						printf("[ERROR] (%s, %d) write failed\n", __func__, __LINE__);
						break;
					}
				}
			} else if(opCode == -1)
				break;
			op_count ++;
			if(op_count % 4000 == 0){
				printCount();
			}
		}
	
		initStat();
		fclose(inputFp);
	} 

	if ( (inputFp = fopen(logFile, "r")) == 0 ) {
		printf("Open File Fail \n");
		exit(1);
	}

	printConf();


	op_count = 0;
	while (1) {
		opCode = trace_parsing2(inputFp,&start_LPN, &length);

		// opCode : operation
		if (opCode == 1) {
			for (i=0 ; i< length  ; i++) {
				if(M_write(start_LPN+i, 0) < 0) {
					printf("[ERROR] (%s, %d) write failed\n", __func__, __LINE__);
					break;
				}
			}
		} else if(opCode == -1)
			break;
		op_count ++;
		if(op_count %2000 == 0){
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
