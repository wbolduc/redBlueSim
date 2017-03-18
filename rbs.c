//William Bolduc 0851313
//rbs.c
//this file does everything except for making, loading, and printing grids
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <smmintrin.h>

#include "wallclock.h"

#include <pthread.h>

#include "grid_t.h"

typedef struct _subSum_t{
	int subSumB;
	int subSumR;
}subSum_t;

typedef struct _threadReturn_t{
	int foundSomething;
	
	int overlayX;
	int overlayY;
	int density;
	int stepsRun;
}threadReturn_t;

typedef struct _threadArgs_t{
	int id;
	int threadCount;
	int maxSteps;
	int maxColour;

	int workStart;
	int workEnd;
	int *exitCond;

	cellLine_t **grid;

	//sectors are the longs which store the colours, the last one may not be full 
	int sectors;
	int partialSectorSize;

	//basically ySize
	int size;
	int overlaySize;

	unsigned long long **blueTransfer;

	subSum_t **subSums;

	pthread_barrier_t *redBarrier;
	pthread_barrier_t *blueBarrier;
	pthread_barrier_t *countBarrier;

	threadReturn_t *returnVal;
}threadArgs_t;

void redShift(cellLine_t *sectors, int fullSectors, int partialSectorSize)
{
	int i;

	register unsigned long long sector0BitCopy;
	register unsigned long long lastSectorCopy;
	register unsigned long long currCopyR;
	register unsigned long long currCoverage;

	register unsigned long long nextLineCoverage;
	
	register unsigned long long lineR;

	register unsigned long long currB;
	register unsigned long long currR;

	register unsigned long long nextB = 0;
	register unsigned long long nextR = 0;

	register unsigned long long canMove;

	register unsigned long long outR;
	register unsigned long long inR = 0;

	currB = (sectors[0]).B;		//I wonder if calling global sector twice is bad, since they are so close anyways
	currR = (sectors[0]).R;
	
	sector0BitCopy = currB | currR;

	//do full shifts
	for (i = 0; i < fullSectors; i++)
	{
		//copy this line
		currCopyR = currR;
		currCoverage = currR | currB;

		//shift above line to line up with it's proceeding chars
		lineR = currR >> 1;
		//shift in bits if there's space
		lineR |= inR;

		//find which cells can Move
		canMove = lineR & ~(currB | currR);
		//add red cells to their moved locations
		currR = currR | canMove;
		//remove red cells from line
		currR &= ~(canMove << 1);

		//remove bit from this sector if the next one has space
		nextR = sectors[i+1].R;
		nextB = sectors[i+1].B;
		nextLineCoverage = nextR | nextB;

		currR &= ~(0x1 & (currCopyR & ~(nextLineCoverage >> (BITCOUNT - 1))));

		//store bit to move if necessary
		inR = ((currCopyR << (BITCOUNT - 1)) | (nextLineCoverage) & 0x80000000);

		//update lines
		(sectors[i]).R = currR;

		currB = nextB;
		currR = nextR;
	}
	
	// shift on partial sector
	nextB = (sectors[i]).B;
	nextR = (sectors[i]).R;
	lastSectorCopy = nextB;

	//place barrier for last shift
	nextB |= 0x4000000000000000 >> (partialSectorSize-1);
	//get bits shifted out		only red because blues wont shift anyways
	outR = nextR & ((unsigned long long)1 << (BITCOUNT - partialSectorSize));
	//shift above line to line up with it's proceeding chars
	lineR = nextR >> 1;
	//shift in bits				only red because blues wont shift
	lineR |= inR;

	//find which cells can Move
	canMove = lineR & ~(nextB | nextR);
	//add red cells to their moved locations
	nextR = nextR | canMove;

	//remove red cells from line
	nextR &= ~(canMove << 1);

	//wrap around to first sector

	//remove last bit if needed
	nextR &= ~((~((sector0BitCopy & (nextR << (partialSectorSize - 1))) | ((nextR & ~sector0BitCopy) << (partialSectorSize - 1)))) >> (partialSectorSize - 1));

	//update lines
	(sectors[i]).R = nextR;		//PROBLEM HERE, MOVING RED INTO LAST ROW IS BORKED

	//add bit to beginning
	sectors[0].R |= (outR << (partialSectorSize - 1)) & ~(sector0BitCopy);
}

void doRedShifts(cellLine_t ** grid, int sectors, int partialSectorSize, int start, int end)
{
	int i;
	for (i = start; i < end; i++)
	{
		redShift(grid[i], sectors, partialSectorSize);
	}
}

void blueShifts(cellLine_t ** grid, int sectors, int start, int end, unsigned long long *blueTransfer, unsigned long long *lastMove)	//movelist is only passed to avoid superfluous mallocs
{
	int i, j;
	
	unsigned long long l1B;
	unsigned long long l1R;
	unsigned long long l2B;
	unsigned long long l2R;
	unsigned long long canMove;

	//move each row at a time
	//do first row, store it's cover in blueTransfer
	
	for(j = 0; j < sectors; j++)
	{
		l1B = grid[start][j].B;
		l1R = grid[start][j].R;

		l2B = grid[start+1][j].B;
		l2R = grid[start+1][j].R;	

		//store first row in blue transfer
		blueTransfer[j] = l1B | l1R;

		//find which cells can Move
		canMove = l1B & ~(l2B | l2R);

		//remove blue cells from line 1
		l1B = l1B & ~canMove;
		//add blue cells to line 2
		l2B = l2B | canMove;

		lastMove[j] = canMove;
		grid[start][j].B = l1B;
		grid[start+1][j].B = l2B;
	}

	end--;
	for(i = start + 1; i < end; i++)
	{
		for(j = 0; j < sectors; j++)
		{
			l1B = grid[i][j].B;

			l2B = grid[i+1][j].B;
			l2R = grid[i+1][j].R;

			//find which cells can move
			canMove = (l1B & ~lastMove[j]) & ~(l2B | l2R);

			//remove blue cells from line 1
			l1B = l1B & ~canMove;

			//add blue cells to line 2
			l2B = l2B | canMove;

			lastMove[j] = canMove;
			grid[i][j].B = l1B;
			grid[i+1][j].B = l2B;
		}
	}
}

int totalSubSums(subSum_t **subSums, int overlayX, int overlayY, int overlaySize, int maxColour)
{
	int i,j,k;
	int sumB = 0;
	int sumR = 0;

	for(i = overlayY * overlaySize; i < overlayY + overlaySize; i++)
	{
		sumB += subSums[i][overlayX].subSumB;
		sumR += subSums[i][overlayX].subSumR;
		if (sumB > maxColour)
		{
			return sumB;
		}
		else if(sumR > maxColour)
		{
			return -sumR;
		}
	}
	return 0;
}

void countSubSums(cellLine_t **grid, int start, int end, subSum_t **subSums, int overlaySize, int size)
{
	int i,j;
	
	int midSector;

	int oLStart;
	
	int startSector= 0;
	int displacement = 0;

	int endSector;
	int remaining;

	int overlayCount = size / overlaySize;

	unsigned long long frontMask;
	unsigned long long rearMask;

	if (overlaySize == BITCOUNT)
	{
		for ( i = start; i < end; i++)
		{
			for (j = 0; j < overlayCount; j++)
			{
				subSums[i][j].subSumB = __popcnt64(grid[i][j].B);
				subSums[i][j].subSumR = __popcnt64(grid[i][j].R);
			}
		}
	}
	else if (overlaySize > BITCOUNT)
	{
		for ( i = start; i < end; i++)
		{
			startSector = 0;
			displacement = 0;
			for (j = 0, oLStart = overlaySize; oLStart <= size; j++, oLStart +=overlaySize)
			{
				endSector = oLStart / BITCOUNT;
				remaining = oLStart % BITCOUNT;

				frontMask = 0xFFFFFFFFFFFFFFFF >> displacement;
				rearMask = 0xFFFFFFFFFFFFFFFF << (BITCOUNT - remaining);	//could be fucky

				//look at first sector
				/*
				printCells(grid[i][startSector], 64);
				printf("\n");
				printBits(grid[i][startSector].B);
				printBits(frontMask);
				printBits(frontMask & grid[i][startSector].B);
				printf("%d\n", __popcnt64(frontMask & grid[i][startSector].B));
				*/

				subSums[i][j].subSumB = __popcnt64(frontMask & grid[i][startSector].B);
				subSums[i][j].subSumR = __popcnt64(frontMask & grid[i][startSector].R);

				//printf("%d\n",subSums[i][j].subSumB);

				//loop though fullSectors
				for(midSector = startSector + 1; midSector < endSector - 1; midSector++)
				{
					subSums[i][j].subSumB += __popcnt64(grid[i][midSector].B);
					subSums[i][j].subSumR += __popcnt64(grid[i][midSector].R);
				}
				/*
				printCells(grid[i][endSector], 64);
				printf("\n");
				printBits(grid[i][endSector].B);
				printBits(rearMask);
				printBits(rearMask & grid[i][endSector].B);
				printf("%d\n", __popcnt64(rearMask & grid[i][endSector].B));
				*/
				//look at last sector
				subSums[i][j].subSumB += __popcnt64(rearMask & grid[i][endSector].B);
				subSums[i][j].subSumR += __popcnt64(rearMask & grid[i][endSector].R);

				//printf("%d\n",subSums[i][j].subSumB);
	

				startSector = endSector;
				displacement = remaining;
				//printf("%d,%d\t",subSums[i][j].subSumR, subSums[i][j].subSumB);
			}
			//printf("\n");
		}
	}
	else
	{
		//overlay is small
		for ( i = start; i < end; i++)
		{
			startSector = 0;
			displacement = 0;
			for (j = 0, oLStart = overlaySize; oLStart <= size; j++, oLStart +=overlaySize)
			{
				endSector = oLStart / BITCOUNT;
				remaining = oLStart % BITCOUNT;

				frontMask = 0xFFFFFFFFFFFFFFFF >> displacement;
				rearMask = 0xFFFFFFFFFFFFFFFF << (BITCOUNT - remaining);	//could be fucky

				if(startSector == endSector)
				{
					subSums[i][j].subSumB = __popcnt64(frontMask & rearMask & grid[i][startSector].B);
					subSums[i][j].subSumR = __popcnt64(frontMask & rearMask & grid[i][startSector].R);

				}
				else
				{
					subSums[i][j].subSumB = __popcnt64(frontMask & grid[i][startSector].B);
					subSums[i][j].subSumR = __popcnt64(frontMask & grid[i][startSector].R);
					subSums[i][j].subSumB += __popcnt64(rearMask & grid[i][endSector].B);
					subSums[i][j].subSumR += __popcnt64(rearMask & grid[i][endSector].R);
				}
			
				startSector = endSector;
				displacement = remaining;
				//printf("%d,%d\t",subSums[i][j].subSumR, subSums[i][j].subSumB);
			}
			//printf("\n");
		}
	}
}

void *shiftThread( void *arguments )
{
	threadArgs_t *args = (threadArgs_t *)arguments;
	int id = args->id;
	int threadCount = args->threadCount;
	int maxSteps = args->maxSteps;

	int maxColour = args->maxColour;

	int sum;
	int* exitCond = args->exitCond;

	cellLine_t **grid = args->grid;
	int fullSectors = args->sectors;
	int partialSectorSize = args->partialSectorSize;
	int totalSectors = fullSectors + 1;
	
	int size = args->size;
	int overlaySize = args->overlaySize;
	int overlayDimension = size / overlaySize;
	int overlayCount = overlayDimension * overlayDimension;
	
	unsigned long long **blueTransfers = args->blueTransfer;
	subSum_t **subSums = args->subSums;

	int start = args->workStart;
	int end = args->workEnd;

	int j,i,step;

	//for blue shifts
	int nextSectorStart;
	int lastRow;
	int blueTransferLine;

	unsigned long long *lastMove;
	unsigned long long l1B;
	unsigned long long l2B;
	unsigned long long l2R;
	unsigned long long canMove;
	pthread_barrier_t *redBarrier = args->redBarrier;
	pthread_barrier_t *blueBarrier = args->blueBarrier;
	pthread_barrier_t *countBarrier = args->countBarrier;

	lastMove = (unsigned long long *)malloc(sizeof(unsigned long long) * totalSectors);
	
	for (step = 0; step < maxSteps; step++)
	{
		//Do red shifts
		doRedShifts(grid, fullSectors, partialSectorSize, start, end);
		pthread_barrier_wait(redBarrier); 
		
		//do blue shifts
		blueShifts(grid, totalSectors, start, end, blueTransfers[id], lastMove);

		//stitch blue transfers together
		//wrap around
		lastRow = end - 1;		
		if (threadCount - 1 == id)
		{
			blueTransferLine = 0;
			nextSectorStart = 0;
		}
		else
		{
			blueTransferLine = id + 1;
			nextSectorStart = end;
		}
		pthread_barrier_wait(blueBarrier);

		//do wrap
		for (j = 0; j < totalSectors; j++)
		{
			l1B = grid[lastRow][j].B;
			l2B = grid[nextSectorStart][j].B;
			//l2R = grid[blueTransferLine][j].R;

			//find which cells can move
			canMove = (l1B & ~lastMove[j]) & ~(blueTransfers[blueTransferLine][j]);
			
			//remove blue cells from line 1
			l1B = l1B & ~canMove;
	
			//add blue cells to line 2
			l2B = l2B | canMove;

			grid[lastRow][j].B = l1B;
			grid[nextSectorStart][j].B = l2B;
		}
		pthread_barrier_wait(blueBarrier);

		//find sub sums
		countSubSums(grid, start, end, subSums, overlaySize, size);
		pthread_barrier_wait(countBarrier);

		for (i = id; i < overlayCount; i += threadCount)
		{
			if((sum = totalSubSums(subSums, i%overlayDimension, i/overlayDimension, overlaySize, maxColour)))
			{
				(*exitCond)++;
				args->returnVal->foundSomething = 1;
				args->returnVal->overlayX = i%overlayDimension;
				args->returnVal->overlayY = i/overlayDimension;
				args->returnVal->density = sum;
				args->returnVal->stepsRun = step + 1;
				break;
			}
			
			if (*exitCond)
			{
				break;
			}
		}
		pthread_barrier_wait(countBarrier);
		if(*exitCond)
		{
			break;
		}
	}

	free(lastMove);
	return NULL;
}

void testRedShift()
{
	int i;
	int fullSectors = 2;
	int partialSectorSize = 10;
	cellLine_t sectors[3];

	sectors[0] = loadColours("B BBRRR ");
	sectors[1] = loadColours(" RRRRRRRRRRRRRRRR ");
	sectors[2] = loadColours("B BBRRR ");
	printCells(sectors[0], 64,stdout);
	printCells(sectors[1], 64,stdout);
	printCells(sectors[2], partialSectorSize, stdout);
	printf("\n");

	for (i = 0; i < 100; i++)
	{
		redShift(sectors, fullSectors, partialSectorSize);
		printCells(sectors[0], 64, stdout);
		printCells(sectors[1], 64, stdout);
		printCells(sectors[2], partialSectorSize,stdout);
		printf("\n");
	}


	//size 10: fullSector = 0, partial = 10
	//size 64: fullSector = 0, partial = 64
	//size 74: fullSector = 1, partial = 10

}

int main(int argc, char *argv[])
{
	//input arguments
	int threadCount = -1;
	int size = -1;
	int overlaySize = -1;
	int cDensity = -1;
	int maxSteps = -1;
	int seed;
	char seedGiven = 0;
	char interactive = 0;

	char notAllArgumentsGiven = 0;	//used to check for all the required command line arguments

	
	int stepsRun;
	int endDensity = 0;
	FILE *outputFile;

	//thread
	pthread_t *threads;
	threadArgs_t *threadArgs;
	pthread_barrier_t redBarrier;
	pthread_barrier_t blueBarrier;
	pthread_barrier_t countBarrier;
	int sectors;
	int cellLineCount;
	int partialSectorSize;
	unsigned long long **blueTransfer;
	subSum_t **subSums;
	
	int exitCond = 0;
	threadReturn_t returnVal;

	int maxColour;
	int workStart, workEnd;
	int i,j,rc;

	float simTime;

	cellLine_t **grid;

	//load in command line arguments
	for(i = 1; argv[i]; i++)
	{
		switch (argv[i][0])
		{
		case 'p':
			threadCount = atoi(&(argv[i][1]));
			break;
		case 'b':
			size = atoi(&(argv[i][1]));
			break;
		case 't':
			overlaySize = atoi(&(argv[i][1]));
			break;
		case 'c':
			cDensity = atoi(&(argv[i][1]));
			break;
		case 'm':
			maxSteps = atoi(&(argv[i][1]));
			break;
		case 's':
			seedGiven = 1;
			seed = atoi(&(argv[i][1]));
			break;
		case 'i':
			interactive = 1;
			break;
		default:
			printf("\"%s\" - is not a valid option", argv[i]);
			break;
		}
	}

	//check required args
	if (threadCount == -1)
	{
		printf("missing processor count\n");
		notAllArgumentsGiven = 1;
	}
	if (size == -1)
	{
		printf("missing board width\n");
		notAllArgumentsGiven = 1;
	}
	if (overlaySize == -1)
	{
		printf("missing overlay tile width\n");
		notAllArgumentsGiven = 1;
	}
	if (cDensity == -1)
	{
		printf("missing colour density\n");
		notAllArgumentsGiven = 1;
	}
	if (maxSteps == -1)
	{
		printf("missing max step count\n");
		notAllArgumentsGiven = 1;
	}
	if (notAllArgumentsGiven)
	{
		getchar();
		return 0;
	}

	//check to make sure overlay fits
	if (size % overlaySize)
	{
		printf("overlay does not fit\n");
		getchar();
		return 0;
	}

	//check for seed then seed
	if (!seedGiven)
	{
		seed = time(NULL);
	}
	srand(seed);
	
	//BUILD DATA STRUCT
	sectors = (size / BITCOUNT);
	partialSectorSize = size % BITCOUNT;

	if (!partialSectorSize)
	{
		partialSectorSize = 64;
		sectors--;
	}

	subSums = (subSum_t **)malloc(sizeof(subSum_t*) * size);
	grid = (cellLine_t **)malloc(sizeof(cellLine_t *) * size); 
	for (i = 0; i < size; i++)
	{
		grid[i] = (cellLine_t*)malloc(sizeof(cellLine_t) * (sectors + 1));
		subSums[i] = (subSum_t *)malloc(sizeof(subSum_t) * (size / overlaySize));
	}
	//initialize them randomly
	randomInitGrid(grid, sectors, size, partialSectorSize);
	
	//printGrid(grid, sectors, size, partialSectorSize);

	blueTransfer = (unsigned long long **)malloc(sizeof(unsigned long long *) * threadCount);
	for (i = 0; i < threadCount; i++)
	{
		blueTransfer[i] = (unsigned long long *)malloc(sizeof(unsigned long long *) * (sectors + 1));
	}

	//initialize return
	returnVal.foundSomething = 0;

	//make barriers
	pthread_barrier_init(&redBarrier, NULL, threadCount);
	pthread_barrier_init(&blueBarrier, NULL, threadCount);
	pthread_barrier_init(&countBarrier, NULL, threadCount);

	//make threads
	threads = (pthread_t *)malloc(sizeof(pthread_t) * threadCount);
	threadArgs = (threadArgs_t *)malloc(sizeof(threadArgs_t) * threadCount);

	maxColour = overlaySize * overlaySize * cDensity / 100;

	workStart = 0;

	StartTime();

	for (i = 0; i < threadCount; i++) {
		//set data for threads
		threadArgs[i].id = i;
		threadArgs[i].threadCount = threadCount;		
		threadArgs[i].maxSteps = maxSteps;
		threadArgs[i].maxColour = maxColour;

		threadArgs[i].exitCond = &exitCond;
		threadArgs[i].returnVal = &returnVal;

		threadArgs[i].grid = grid;
		threadArgs[i].size = size;
		threadArgs[i].overlaySize = overlaySize;
		threadArgs[i].sectors = sectors;
		threadArgs[i].partialSectorSize = partialSectorSize;
		
		threadArgs[i].blueTransfer = blueTransfer;
		
		threadArgs[i].redBarrier = &redBarrier;
		threadArgs[i].blueBarrier = &blueBarrier;
		threadArgs[i].countBarrier = &countBarrier;

		threadArgs[i].subSums = subSums;

		//assign work
		workEnd = workStart + size/threadCount;
		if (size % threadCount > i)
		{
			workEnd++;
		}
		threadArgs[i].workStart = workStart;
		threadArgs[i].workEnd = workEnd;
		workStart = workEnd;

		//make threads
        if ((rc = pthread_create(&threads[i], NULL, shiftThread, &threadArgs[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            return EXIT_FAILURE;
        }
    }

	//block until simulation is complete
	for (i = 0; i < threadCount; i++) {
		pthread_join(threads[i], NULL);
	}
	simTime = EndTime();

	//output
	stepsRun = maxSteps;
	endDensity = 0;
	if (returnVal.foundSomething)
	{
		stepsRun = returnVal.stepsRun;
		endDensity = abs(returnVal.density) * 100 / (overlaySize * overlaySize);
	}

	outputFile = fopen("redblue.txt","w");
	printGrid(grid, sectors, size, partialSectorSize, outputFile);
	fprintf(outputFile, "p%d b%d t%d c%d m%d s%d Iterations: %d density %d Timer: %lf", threadCount, size, overlaySize, cDensity, maxSteps, seed, stepsRun, endDensity, simTime);
	fclose(outputFile);

	printf("p%d b%d t%d c%d m%d s%d Iterations: %d density %d Timer: %lf", threadCount, size, overlaySize, cDensity, maxSteps, seed, stepsRun, endDensity, simTime);
	//free data
	for (i = 0; i < size; i++)
	{
		free(subSums[i]);
		free(grid[i]);
	}
	free(grid);
	free(subSums);

	for (i = 0; i < threadCount; i++)
	{
		free(blueTransfer[i]);
	}
	free(blueTransfer);
	
	free(threadArgs);
	free(threads);
	
	pthread_barrier_destroy(&redBarrier);
	pthread_barrier_destroy(&blueBarrier);
	pthread_barrier_destroy(&countBarrier);
	
	getchar();
	return 0;
}