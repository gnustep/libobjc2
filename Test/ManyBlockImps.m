#include "../objc/runtime.h"
#include "../objc/blocks_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Sets and unsets a few thousand blocks as IMPs.  One set of trampolines holds
 * one page worth of them (128 on AArch64 and 256 on x86-64 with 4 KiB pages),
 * so the runtime needs many sets and must reuse the trampolines that are freed
 * in any of them.
 */

#define COUNT 5000

struct big
{
	int a, b, c, d, e;
};

/*
 * Makes an IMP from a heap copy of a block, and leaves the IMP with the only
 * reference to that copy.
 */
static IMP heapBlockIMP(id stackBlock)
{
	id block = Block_copy(stackBlock);
	IMP imp = imp_implementationWithBlock(block);
	assert(imp != NULL);
	assert(imp_getBlock(imp) == block);
	Block_release(block);
	return imp;
}

/*
 * Returns an IMP that returns value, or a struct big that starts with value.
 */
static IMP makeIMP(int value, BOOL sret)
{
	if (sret)
	{
		struct big (^block)(id) = ^(id self)
		{
			struct big b = { value, value + 1, value + 2, value + 3, value + 4 };
			return b;
		};
		return heapBlockIMP(block);
	}
	int (^block)(id) = ^(id self) { return value; };
	return heapBlockIMP(block);
}

static int checkBig(struct big b)
{
	assert(b.b == b.a + 1);
	assert(b.c == b.a + 2);
	assert(b.d == b.a + 3);
	assert(b.e == b.a + 4);
	return b.a;
}

static void checkIMP(IMP imp, int value, BOOL sret)
{
	id block = imp_getBlock(imp);
	assert(block != nil);
	if (sret)
	{
		assert(checkBig(((struct big(*)(id, SEL))imp)(nil, @selector(big))) == value);
		assert(checkBig(((struct big(^)(id))block)(nil)) == value);
	}
	else
	{
		assert(((int(*)(id, SEL))imp)(nil, @selector(value)) == value);
		assert(((int(^)(id))block)(nil) == value);
	}
}

static void removeIMP(IMP imp)
{
	assert(imp_removeBlock(imp));
	assert(imp_getBlock(imp) == nil);
	// Removing it twice must not put the trampoline on the free list twice.
	assert(!imp_removeBlock(imp));
}

static int compareIMPs(const void *a, const void *b)
{
	uintptr_t x = (uintptr_t)*(const IMP *)a;
	uintptr_t y = (uintptr_t)*(const IMP *)b;
	return (x > y) - (x < y);
}

static void assertDistinct(IMP *imps)
{
	IMP *sorted = calloc(COUNT, sizeof(IMP));
	memcpy(sorted, imps, COUNT * sizeof(IMP));
	qsort(sorted, COUNT, sizeof(IMP), compareIMPs);
	for (int i=1 ; i<COUNT ; i++)
	{
		assert(sorted[i-1] != sorted[i]);
	}
	free(sorted);
}

static void test(BOOL sret)
{
	IMP *imps = calloc(COUNT, sizeof(IMP));
	int *values = calloc(COUNT, sizeof(int));
	IMP *freed = calloc(COUNT, sizeof(IMP));
	int freedCount = 0;

	for (int i=0 ; i<COUNT ; i++)
	{
		values[i] = i;
		imps[i] = makeIMP(values[i], sret);
	}
	assertDistinct(imps);
	for (int i=0 ; i<COUNT ; i++)
	{
		checkIMP(imps[i], values[i], sret);
	}

	// Remove every other IMP, so that every set has free trampolines.
	for (int i=1 ; i<COUNT ; i+=2)
	{
		removeIMP(imps[i]);
		freed[freedCount++] = imps[i];
	}
	for (int i=0 ; i<COUNT ; i+=2)
	{
		checkIMP(imps[i], values[i], sret);
	}

	// Make as many new IMPs.  Only the newest set can still have trampolines
	// that were never used, so most of them must reuse freed trampolines.
	qsort(freed, freedCount, sizeof(IMP), compareIMPs);
	int reused = 0;
	for (int i=1 ; i<COUNT ; i+=2)
	{
		values[i] = COUNT + i;
		imps[i] = makeIMP(values[i], sret);
		if (bsearch(&imps[i], freed, freedCount, sizeof(IMP), compareIMPs))
		{
			reused++;
		}
	}
	assert(reused >= freedCount / 2);
	assertDistinct(imps);
	for (int i=0 ; i<COUNT ; i++)
	{
		checkIMP(imps[i], values[i], sret);
	}

	// Remove them all, then do it all again with every trampoline free.
	for (int i=0 ; i<COUNT ; i++)
	{
		removeIMP(imps[i]);
	}
	for (int i=0 ; i<COUNT ; i++)
	{
		values[i] = 2 * COUNT + i;
		imps[i] = makeIMP(values[i], sret);
	}
	assertDistinct(imps);
	for (int i=0 ; i<COUNT ; i++)
	{
		checkIMP(imps[i], values[i], sret);
	}
	for (int i=0 ; i<COUNT ; i++)
	{
		removeIMP(imps[i]);
	}

	free(imps);
	free(values);
	free(freed);
}

int main(void)
{
	test(NO);
	// Blocks that return structures in memory use their own trampolines on
	// targets that pass the return slot as the first argument (e.g. x86-64).
	test(YES);
	return 0;
}
