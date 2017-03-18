//William Bolduc 0851313
//grid_t.h
//making, loading, and printing grids

#define BITCOUNT 64
#include <stdio.h>

typedef struct _cellLine_t{
	unsigned long long R;
	unsigned long long B;
}cellLine_t;

typedef struct _rbGrid_t{
	cellLine_t **grid;

	//sectors are the longs which store the colours, the last one may not be full 
	int sectors;
	int partialSectorSize;

	//basically ySize
	int size;

	unsigned long long **blueTransfer;
	//x size = sectors
	int yTransfers;
}rbGrid_t;


void randomInitGrid(cellLine_t **grid, int sectors, int size, int partialSectorSize);		//I wrote this tired, I'm sorry guys

void printGrid(cellLine_t **grid, int sectors, int size, int partialSectorSize, FILE *out);

void printCellLineGrid(cellLine_t ** grid, int size, int fullSectors, int partialSectorSize);	//TODO: fix this for output

rbGrid_t *makeGrid(int size, int threadCount);

void freeGrid(rbGrid_t * grid);

void printBits(unsigned long long x);

void printCellBits(cellLine_t line);

void printCells(cellLine_t line, int partialSectorSize, FILE *out);

cellLine_t loadColours(char line[]);