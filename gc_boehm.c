#include "objc/runtime.h"
#include "visibility.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gc_ops.h"
#define I_HIDE_POINTERS
#include <gc.h>
#include <gc/gc_typed.h>

#ifndef __clang__
#define __sync_swap __sync_lock_test_and_set
#endif

enum
{
	OBJC_RATIO_COLLECTION        = 0,
	OBJC_GENERATIONAL_COLLECTION = 1,
	OBJC_FULL_COLLECTION         = 2,
	OBJC_EXHAUSTIVE_COLLECTION   = 3,
	OBJC_COLLECT_IF_NEEDED       = (1 << 3),
	OBJC_WAIT_UNTIL_DONE         = (1 << 4),
};

enum
{
	OBJC_CLEAR_RESIDENT_STACK = 1
};

static unsigned long collectionType(unsigned options)
{
	// Low 2 bits in GC options are used for the 
	return options & 3;
}


void objc_collect(unsigned long options)
{
	if (OBJC_FULL_COLLECTION == collectionType(options))
	{
		GC_gcollect();
	}
	else
	{
		GC_collect_a_little();
	}
}

BOOL objc_collectingEnabled(void)
{
	return YES;
}

BOOL objc_atomicCompareAndSwapPtr(id predicate, id replacement, volatile id *objectLocation)
{
	return __sync_bool_compare_and_swap(objectLocation, predicate, replacement);
}
BOOL objc_atomicCompareAndSwapPtrBarrier(id predicate, id replacement, volatile id *objectLocation)
{
	return __sync_bool_compare_and_swap(objectLocation, predicate, replacement);
}

BOOL objc_atomicCompareAndSwapGlobal(id predicate, id replacement, volatile id *objectLocation)
{
	return objc_atomicCompareAndSwapPtr(predicate, replacement, objectLocation);
}
BOOL objc_atomicCompareAndSwapGlobalBarrier(id predicate, id replacement, volatile id *objectLocation)
{
	return objc_atomicCompareAndSwapPtr(predicate, replacement, objectLocation);
}
BOOL objc_atomicCompareAndSwapInstanceVariable(id predicate, id replacement, volatile id *objectLocation)
{
	return objc_atomicCompareAndSwapPtr(predicate, replacement, objectLocation);
}
BOOL objc_atomicCompareAndSwapInstanceVariableBarrier(id predicate, id replacement, volatile id *objectLocation)
{
	return objc_atomicCompareAndSwapPtr(predicate, replacement, objectLocation);
}


id objc_assign_strongCast(id val, id *ptr)
{
	GC_change_stubborn(ptr);
	*ptr = val;
	GC_end_stubborn_change(ptr);
	return val;
}

id objc_assign_global(id val, id *ptr)
{
	*ptr = val;
	return val;
}
id objc_assign_ivar(id val, id dest, ptrdiff_t offset)
{
	GC_change_stubborn(dest);
	*(id*)((char*)dest+offset) = val;
	GC_end_stubborn_change(dest);
	return val;
}
void *objc_memmove_collectable(void *dst, const void *src, size_t size)
{
	// FIXME: Does this need to be called with the allocation lock held?
	memmove(dst, src, size);
	return dst;
}
/**
 * Weak Pointers:
 *
 * To implement weak pointers, we store the hidden pointer (bits all flipped)
 * in the real address.  We tell the GC to zero the pointer when the associated
 * object is finalized.  The read barrier locks the GC to prevent it from
 * freeing anything, deobfuscates the pointer (at which point it becomes a
 * GC-visible on-stack pointer), and then returns it.
 */

static void *readWeakLocked(void *ptr)
{
	void *val = *(void**)ptr;
	return 0 == val ? val : REVEAL_POINTER(val);
}

id objc_read_weak(id *location)
{
	return GC_call_with_alloc_lock(readWeakLocked, location);
}

id objc_assign_weak(id value, id *location)
{

	// Temporarily zero this pointer and get the old value
	id old = __sync_swap(location, 0);
	if (0 != old)
	{
		GC_unregister_disappearing_link((void**)location);
	}
	GC_general_register_disappearing_link((void**)location, value);
	// If some other thread has modified this, then we may have two different
	// objects registered to make this pointer 0 if either is destroyed.  This
	// would be bad, so we need to make sure that we unregister them and
	// register the correct one.
	if (!__sync_bool_compare_and_swap(location, old, (id)HIDE_POINTER(value)))
	{
		return objc_assign_weak(value, location);
	}
	else
	{
		//fprintf(stderr, "Done weak assignment\n");
	}
	return value;
}

static void runFinalize(void *addr, void *context)
{
	static SEL finalize;
	if (UNLIKELY(0 == finalize))
	{
		finalize = sel_registerName("finalize");
	}
	objc_msg_lookup(addr, finalize)(addr, finalize);
}

static id allocate_class(Class cls, size_t extra)
{
	id obj;
	if (extra > 0)
	{
		// FIXME: Overflow checking!
		obj = GC_malloc(class_getInstanceSize(cls) + extra);
	}
	else
	{
		obj = GC_malloc_stubborn(class_getInstanceSize(cls));
	}
	GC_register_finalizer_no_order(obj, runFinalize, 0, 0, 0);
	return obj;
}

id objc_allocate_object(Class cls, int extra)
{
	return class_createInstance(cls, extra);
}

static void registerThread(BOOL errorOnNotRegistered)
{
	struct GC_stack_base base;
	if (GC_get_stack_base(&base) != GC_SUCCESS)
	{
		fprintf(stderr, "Unable to find stack base for new thread\n");
		abort();
	}
	switch (GC_register_my_thread(&base))
	{
		case GC_SUCCESS:
			if (errorOnNotRegistered)
			{
				fprintf(stderr, "Thread should have already been registered with the GC\n");
			}
		case GC_DUPLICATE:
			return;
		case GC_NO_THREADS:
		case GC_UNIMPLEMENTED:
			fprintf(stderr, "Unable to register stack\n");
			abort();
	}
}

void objc_registerThreadWithCollector(void)
{
	registerThread(NO);
}
void objc_unregisterThreadWithCollector(void)
{
	GC_unregister_my_thread();
}
void objc_assertRegisteredThreadWithCollector()
{
	registerThread(YES);
}

/**
 * Structure stored for each GC
 */
struct gc_refcount
{
	/** Reference count */
	int refCount;
	/** Strong pointer */
	id ptr;
};

static int refcount_compare(const void *ptr, const struct gc_refcount *rc)
{
	return ptr == rc->ptr;
}
static uint32_t ptr_hash(const void *ptr)
{
	// Bit-rotate right 4, since the lowest few bits in an object pointer will
	// always be 0, which is not so useful for a hash value
	return ((uintptr_t)ptr >> 4) | ((uintptr_t)ptr << (sizeof(id) * 8) - 4);
}
static uint32_t refcount_hash(const struct gc_refcount *rc)
{
	return ptr_hash(rc->ptr);
}
#define MAP_TABLE_NAME refcount
#define MAP_TABLE_COMPARE_FUNCTION refcount_compare
#define MAP_TABLE_HASH_KEY ptr_hash
#define MAP_TABLE_HASH_VALUE refcount_hash
#include "hash_table.h"

static refcount_table *refcounts;

id objc_gc_retain_np(id object)
{
	struct gc_refcount *refcount = refcount_table_get(refcounts, object);
	if (NULL == refcount)
	{
		LOCK_FOR_SCOPE(&(refcounts->lock));
		refcount = refcount_table_get(refcounts, object);
		if (NULL == refcount)
		{
			refcount = GC_malloc_uncollectable(sizeof(struct gc_refcount));
			refcount->ptr = object;
			refcount->refCount = 1;
			refcount_insert(refcounts, refcount);
			return object;
		}
	}
	__sync_fetch_and_add(&(refcount->refCount), 1);
	return object;
}
void objc_gc_release_np(id object)
{
	struct gc_refcount *refcount = refcount_table_get(refcounts, object);
	// This object has not been explicitly retained, don't release it
	if (0 == refcount) { return; }
	
	if (0 == __sync_sub_and_fetch(&(refcount->refCount), 1))
	{
		LOCK_FOR_SCOPE(&(refcounts->lock));
		refcount_remove(refcounts, object);
		__sync_synchronize();
		// If another thread has incremented the reference count while we were
		// doing this, then we need to add the count back into the table,
		// otherwise we can carry on.
		if (__sync_bool_compare_and_swap(&(refcount->refCount), 0, 0))
		{
			// This doesn't free the object, it just removes the explicit
			// reference
			GC_free(refcount);
		}
		else
		{
			refcount_insert(refcounts, refcount);
		}
	}
}

static GC_descr UnscannedDescr;

void* objc_gc_allocate_collectible(size_t size, BOOL isScanned)
{
	if (isScanned)
	{
		return GC_malloc(size);
	}
	return GC_malloc_explicitly_typed(size, UnscannedDescr);
}



static void init(void)
{
	refcounts = refcount_create(4096);
	GC_word bitmap = 0;
	UnscannedDescr = GC_make_descriptor(&bitmap, 1);
}

// FIXME: These are all stub implementations that should be replaced with
// something better
BOOL objc_is_finalized(void *ptr) { return NO; }
void objc_clear_stack(unsigned long options)
{
	// This isn't a very good implementation - we should really be working out
	// how much stack space is left somehow, but this is not possible to do
	// portably.
	int i[1024];
	memset(&i, 0, 1024);
}
BOOL objc_collecting_enabled(void) { return NO; }
void objc_set_collection_threshold(size_t threshold) {}
void objc_set_collection_ratio(size_t ratio) {} 
void objc_start_collector_thread(void) {}
void objc_finalizeOnMainThread(Class cls) {}
void objc_setCollectionThreshold(size_t threshold) {}
void objc_startCollectorThread(void) {}


PRIVATE struct gc_ops gc_ops_boehm = 
{
	.allocate_class = allocate_class,
	.init = init
};

PRIVATE void enableGC(BOOL exclude)
{
	if (__sync_bool_compare_and_swap(&gc, &gc_ops_none, &gc_ops_boehm))
	{
		gc->init();
	}
}
