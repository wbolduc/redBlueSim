//William Bolduc 0851313
//grid_t.c
//making, loading, and printing grids

#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "grid_t.h"

void randomInitGrid(cellLine_t **grid, int fullSectors, int size, int partialSectorSize)		//I wrote this tired, I'm sorry guys
{
	int i, j, k, colour;

	for (i = 0; i < size; i++)
	{
		//load full sectors
		for (j = 0; j < fullSectors; j++)
		{
			grid[i][j].R = 0;
			grid[i][j].B = 0;
			for(k = 0; k < BITCOUNT - 1; k++)
			{
				colour = rand() % 3;
				switch(colour)
				{
				case 1:
					grid[i][j].R |= 1;
					break;
				case 2:
					grid[i][j].B |= 1;
					break;
				case 0:
					break;
				}
				grid[i][j].R <<= 1;
				grid[i][j].B <<= 1;
			}

			//load last colour in this sector
			colour = rand() % 3;
			switch(colour)
			{
			case 1:
				grid[i][j].R |= 1;
				break;
			case 2:
				grid[i][j].B |= 1;
				break;
			case 0:
				break;
			}
		}
		
		//load partial sector
		grid[i][j].R = 0;
		grid[i][j].B = 0;
		for(k = 0; k < partialSectorSize - 1; k++)
		{
			colour = rand() % 3;
			switch(colour)
			{
			case 1:
				grid[i][j].R |= 1;
				break;
			case 2:
				grid[i][j].B |= 1;
				break;
			case 0:
				break;
			}
			grid[i][j].R <<= 1;
			grid[i][j].B <<= 1;
		}
		//load last colour in partial sector
		colour = rand() % 3;
		switch(colour)
		{
		case 1:
			grid[i][j].R |= 1;
			break;
		case 2:
			grid[i][j].B |= 1;
			break;
		case 0:
			break;
		}
		grid[i][j].R <<= (BITCOUNT - partialSectorSize);
		grid[i][j].B <<= (BITCOUNT - partialSectorSize);
	}
}

void printGrid(cellLine_t **grid, int sectors, int size, int partialSectorSize, FILE *out)
{
	int i,j;

	for (i = 0; i < size; i++)
	{
		//print full sectors
		for(j = 0; j < sectors; j++)
		{
			printCells(grid[i][j], BITCOUNT, out);
		}
		//print partial sectors
		printCells(grid[i][j], partialSectorSize, out);		//TODO: FIX THIS

		fprintf(out, "\n");
	}
}

void printCellLineGrid(cellLine_t ** grid, int size, int fullSectors, int partialSectorSize)	//TODO: fix this for output
{
	int i,j;

	for (i = 0; i < size; i++)
	{
		//print full sectors
		for(j = 0; j < fullSectors; j++)
		{
			printCells(grid[i][j], BITCOUNT, stdout);
		}
		//print partial sectors
		printCells(grid[i][j], partialSectorSize, stdout);		//TODO: FIX THIS

		printf("\n");
	}
}

rbGrid_t *makeGrid(int size, int threadCount)
{
	int i;
	rbGrid_t *grid;

	grid = (rbGrid_t *)malloc(sizeof(rbGrid_t));

	grid->size = size;
	grid->sectors = (size / BITCOUNT) + 1;
	grid->partialSectorSize = size % BITCOUNT;

	grid->yTransfers = threadCount;

	grid->grid = (cellLine_t **)malloc(sizeof(cellLine_t *) * size); 
	for (i = 0; i < size; i++)
	{
		grid->grid[i] = (cellLine_t*)malloc(sizeof(cellLine_t) * size);
	}

	grid->blueTransfer = (unsigned long long **)malloc(sizeof(unsigned long long *) * threadCount);
	for (i = 0; i < threadCount; i++)
	{
		grid->blueTransfer[i] = (unsigned long long *)malloc(sizeof(unsigned long long *) * size);
	}
	return grid;
}

void freeGrid(rbGrid_t * grid)
{
	int i;
	for(i = 0; i < grid->size; i++)
	{
		free(grid->grid[i]);
		free(grid->blueTransfer[i]);
	}
	free(grid->grid);
	free(grid->blueTransfer);
	free(grid);
}

void printBits(unsigned long long x)
{
	int bytes;
	unsigned char *b = (unsigned char *)&x;
	int i, j;

	bytes = sizeof(x);
	for (i = bytes - 1; i >= 0; i--)
	{
		for (j = 7; j >= 0; j--)
		{
			printf("%u", b[i] >> j & 0x1);
		}
	}
	printf("|\n");
}

void printCellBits(cellLine_t line)
{
	printBits(line.R);
	printBits(line.B);
}

void printCells(cellLine_t line, int partialSectorSize, FILE * out)
{
	int bytes;
	int i, j;
	unsigned char *b = (unsigned char *)&line.B;
	unsigned char *r = (unsigned char *)&line.R;
	
	if (!partialSectorSize)
	{
		return;
	}

	bytes = sizeof(line.B);

	for (i = bytes - 1; i >= 0; i--)
	{
		for (j = 7; j >= 0; j--)
		{
			if (b[i] >> j & 0x1)
			{
				fprintf(out, "V");
			}
			else if (r[i] >> j & 0x1)
			{
				fprintf(out, ">");
			}
			else
			{
				fprintf(out, " ");
			}

			if(!--partialSectorSize)
			{
				return;
			}
		}
	}
}



cellLine_t loadColours(char line[])
{
	int i, max;
	cellLine_t bitLine;
	
	bitLine.R = 0;
	bitLine.B = 0;

	max = strlen(line);

	if (max > BITCOUNT)
	{
		max = BITCOUNT;
	}
	
	for(i = 0; i < max; i++)
	{
		if (line[i] == 'B')
		{
			bitLine.B |= 0X1;
		}
		else if (line[i] == 'R')
		{
			bitLine.R |= 0X1;
		}
		
		if (i < max - 1)
		{
			bitLine.B <<= 1;
			bitLine.R <<= 1;
		}
	}
	bitLine.B <<= BITCOUNT - max;
	bitLine.R <<= BITCOUNT - max;
	return bitLine;
}