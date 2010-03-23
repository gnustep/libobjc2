#include "objc/runtime.h"
#include <stdio.h>
#include <unistd.h>

// Subset of NSObject interface needed for properties.
@interface NSObject {}
- (id)retain;
- (id)copy;
- (id)autorelease;
- (void)release;
@end

/**
 * Number of spinlocks.  This allocates one page on 32-bit platforms.
 */
#define spinlock_count (1<<10)
const int spinlock_mask = spinlock_count - 1;
/**
 * Integers used as spinlocks for atomic property access.
 */
static int spinlocks[spinlock_count];
/**
 * Get a spin lock from a pointer.  We want to prevent lock contention between
 * properties in the same object - if someone is stupid enough to be using
 * atomic property access, they are probably stupid enough to do it for
 * multiple properties in the same object.  We also want to try to avoid
 * contention between the same property in different objects, so we can't just
 * use the ivar offset.
 */
static inline int *lock_for_pointer(void *ptr)
{
	intptr_t hash = (intptr_t)ptr;
	// Most properties will be pointers, so disregard the lowest few bits
	hash >>= sizeof(void*) == 4 ? 2 : 8;
	intptr_t low = hash & spinlock_mask;
	hash >>= 16;
	hash |= low;
	return spinlocks + (hash & spinlock_mask);
}

inline static void unlock_spinlock(int *spinlock)
{
	*spinlock = 0;
}
inline static void lock_spinlock(int *spinlock)
{
	int count = 0;
	// Set the spin lock value to 1 if it is 0.
	while(!__sync_bool_compare_and_swap(spinlock, 0, 1))
	{
		count++;
		if (0 == count % 10)
		{
			// If it is already 1, let another thread play with the CPU for a
			// bit then try again.
			sleep(0);
		}
	}
}

id objc_getProperty(id obj, SEL _cmd, int offset, BOOL isAtomic)
{
	char *addr = (char*)obj;
	addr += offset;
	id ret;
	if (isAtomic)
	{
		int *lock = lock_for_pointer(addr);
		lock_spinlock(lock);
		ret = *(id*)addr;
		ret = [ret retain];
		unlock_spinlock(lock);
	}
	else
	{
		ret = *(id*)addr;
		ret = [ret retain];
	}
	return [ret autorelease];
}

void objc_setProperty(id obj, SEL _cmd, int offset, id arg, BOOL isAtomic, BOOL isCopy)
{
	if (isCopy)
	{
		arg = [arg copy];
	}
	else
	{
		arg = [arg retain];
	}
	char *addr = (char*)obj;
	addr += offset;
	id old;
	if (isAtomic)
	{
		int *lock = lock_for_pointer(addr);
		lock_spinlock(lock);
		old = *(id*)addr;
		*(id*)addr = arg;
		unlock_spinlock(lock);
	}
	else
	{
		old = *(id*)addr;
		*(id*)addr = arg;
	}
	[old release];
}
