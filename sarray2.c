#include <stdlib.h>
#ifdef BUILD_TESTS
#include <stdio.h>
#endif

#include "sarray2.h"

static void *EmptyArrayData[256];
static SparseArray EmptyArray = { 0, 0xff, (void**)&EmptyArrayData};

static void init_pointers(SparseArray * sarray)
{
	sarray->data = calloc(256, sizeof(void*));
	if(sarray->shift != 0)
	{
		for(unsigned i=0 ; i<256 ; i++)
		{
			sarray->data[i] = &EmptyArray;
		}
	}
}

SparseArray * SparseArrayNew()
{
	SparseArray * sarray = calloc(1, sizeof(SparseArray));
	sarray->shift = 24;
	sarray->mask = 0xff000000;
	init_pointers(sarray);
	return sarray;
}


void * SparseArrayNext(SparseArray * sarray, uint32_t * index)
{
	uint32_t j = MASK_INDEX((*index));
	uint32_t max = (sarray->mask >> sarray->shift) + 1;
	if(sarray->shift == 0)
	{
		while(j<max)
		{
			(*index)++;
			if(sarray->data[j] != SARRAY_EMPTY)
			{
				return sarray->data[j];
			}
			j++;
		}
	}
	else while(j<max)
	{
		uint32_t zeromask = ~(sarray->mask >> 8);
		while(j<max)
		{
			//Look in child nodes
			if(sarray->data[j] != SARRAY_EMPTY)
			{
				void * ret = SparseArrayNext(sarray->data[j], index);
				if(ret != SARRAY_EMPTY)
				{
					return ret;
				}
			}
			//Go to the next child
			j++;
			//Add 2^n to index so j is still correct
			(*index) += 1<<sarray->shift;
			//Zero off the next component of the index so we don't miss any.
			*index &= zeromask;
		}
	}
	return SARRAY_EMPTY;
}

void SparseArrayInsert(SparseArray * sarray, uint32_t index, void * value)
{
	while(sarray->shift > 0)
	{
		uint32_t i = MASK_INDEX(index);
		if(sarray->data[i] == &EmptyArray)
		{
			SparseArray * newsarray = calloc(1, sizeof(SparseArray));
			newsarray->shift = sarray->shift - 8;
			newsarray->mask = sarray->mask >> 8;
			init_pointers(newsarray);
			sarray->data[i] = newsarray;
		}
		sarray = sarray->data[i];
	}
	sarray->data[index & sarray->mask] = value;
}

void SparseArrayDestroy(SparseArray * sarray)
{
	if(sarray->shift > 0)
	{
		uint32_t max = (sarray->mask >> sarray->shift) + 1;
		for(uint32_t i=0 ; i<max ; i++)
		{
			SparseArrayDestroy((SparseArray*)sarray->data[i]);
		}
	}
	free(sarray->data);
	free(sarray);
}

