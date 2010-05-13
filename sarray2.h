/**
 * Sparse Array
 *
 * Author: David Chisnall
 * 
 * License: See COPYING.MIT
 *
 */

#ifndef _SARRAY_H_INCLUDED_
#define _SARRAY_H_INCLUDED_
#include <stdint.h>
#include <stdlib.h>

/**
 * Sparse arrays, used to implement dispatch tables.  Current implementation is
 * quite RAM-intensive and could be optimised.  Maps 32-bit integers to pointers.
 *
 * Note that deletion from the array is not supported.  This allows accesses to
 * be done without locking; the worst that can happen is that the caller gets
 * an old value (and if this is important to you then you should be doing your
 * own locking).  For this reason, you should be very careful when deleting a
 * sparse array that there are no references to it held by other threads.
 */
typedef struct 
{
	uint32_t mask;
	uint32_t shift;
	void ** data;
} SparseArray;

/**
 * Turn an index in the array into an index in the current depth.
 */
#define MASK_INDEX(index) \
	((index & sarray->mask) >> sarray->shift)

#define SARRAY_EMPTY ((void*)0)
/**
 * Look up the specified value in the sparse array.  This is used in message
 * dispatch and so has been put in the header to allow compilers to inline it,
 * even though this breaks the abstraction.
 */
static inline void* SparseArrayLookup(SparseArray * sarray, uint32_t index)
{
	while(sarray->shift > 0)
	{
		uint32_t i = MASK_INDEX(index);
		sarray = (SparseArray*) sarray->data[i];
	}
	uint32_t i = index & sarray->mask;
	return sarray->data[i];
}
/**
 * Create a new sparse array.
 */
SparseArray * SparseArrayNew();
/**
 * Insert a value at the specified index.
 */
void SparseArrayInsert(SparseArray * sarray, uint32_t index, void * value);
/**
 * Destroy the sparse array.  Note that calling this while other threads are
 * performing lookups is guaranteed to break.
 */
void SparseArrayDestroy(SparseArray * sarray);
/**
 * Iterate through the array.  Returns the next non-NULL value after index and
 * sets index to the following value.  For example, an array containing values
 * at 0 and 10 will, if called with index set to 0 first return the value at 0
 * and set index to 1.  A subsequent call with index set to 1 will return the
 * value at 10 and set index to 11.
 */
void * SparseArrayNext(SparseArray * sarray, uint32_t * index);

#endif //_SARRAY_H_INCLUDED_
