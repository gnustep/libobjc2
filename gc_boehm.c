#define GNUSTEP_LIBOBJC_NO_LEGACY
#include "objc/runtime.h"
#include "objc/toydispatch.h"
#include "class.h"
#include "ivar.h"
#include "lock.h"
#include "objc/objc-auto.h"
#include "visibility.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "gc_ops.h"
#define I_HIDE_POINTERS


/**
 * Dispatch queue used to invoke finalizers.
 */
static dispatch_queue_t finalizer_queue;
/**
 * Should finalizers be invoked in their own thread?
 */
static BOOL finalizeThreaded;


/*
 * Citing boehm-gc's README.linux:
 *
 * 3a) Every file that makes thread calls should define GC_LINUX_THREADS and
 *  _REENTRANT and then include gc.h.  Gc.h redefines some of the
 *  pthread primitives as macros which also provide the collector with
 *  information it requires.
 */
#ifdef __linux__
# define GC_LINUX_THREADS

# ifndef _REENTRANT
#  define _REENTRANT
# endif

#endif
#include <gc/gc.h>
#include <gc/gc_typed.h>

#ifndef __clang__
#define __sync_swap __sync_lock_test_and_set
#endif

Class dead_class;

Class objc_lookup_class(const char*);

GC_descr gc_typeForClass(Class cls);
void gc_setTypeForClass(Class cls, GC_descr type);

static unsigned long collectionType(unsigned options)
{
	// Low 2 bits in GC options are used for the
	return options & 3;
}

static size_t CollectRatio     = 0x10000;
static size_t CollectThreshold = 0x10000;

void objc_set_collection_threshold(size_t threshold)
{
	CollectThreshold = threshold;
}
void objc_set_collection_ratio(size_t ratio)
{
	CollectRatio = ratio;
}

void objc_collect(unsigned long options)
{
	size_t newAllocations = GC_get_bytes_since_gc();
	// Skip collection if we haven't allocated much memory and this is a
	// collect if needed collection
	if ((options & OBJC_COLLECT_IF_NEEDED) && (newAllocations < CollectThreshold))
	{
		return;
	}
	switch (collectionType(options))
	{
		case OBJC_RATIO_COLLECTION:
			if (newAllocations >= CollectRatio)
			{
				GC_gcollect();
			}
			else
			{
				GC_collect_a_little();
			}
			break;
		case OBJC_GENERATIONAL_COLLECTION:
			GC_collect_a_little();
			break;
		case OBJC_FULL_COLLECTION:
			GC_gcollect();
			break;
		case OBJC_EXHAUSTIVE_COLLECTION:
		{
			size_t freeBytes = 0;
			while (GC_get_free_bytes() != freeBytes)
			{
				freeBytes = GC_get_free_bytes();
				GC_gcollect();
			}
		}
	}
}

BOOL objc_collectingEnabled(void)
{
	return GC_dont_gc == 0;
}

void objc_gc_disable(void)
{
	GC_disable();
}
void objc_gc_enable(void)
{
	GC_enable();
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
	*ptr = val;
	return val;
}

id objc_assign_global(id val, id *ptr)
{
	//fprintf(stderr, "Storign %p in global %p\n", val, ptr);
	GC_add_roots(ptr, ptr+1);
	*ptr = val;
	return val;
}

id objc_assign_ivar(id val, id dest, ptrdiff_t offset)
{
	*(id*)((char*)dest+offset) = val;
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
	// If the value is not GC'd memory (e.g. a class), the collector will crash
	// trying to collect it when you add it as the target of a disappearing
	// link.
	if (0 != GC_base(value))
	{
		GC_GENERAL_REGISTER_DISAPPEARING_LINK((void**)location, value);
	}
	// If some other thread has modified this, then we may have two different
	// objects registered to make this pointer 0 if either is destroyed.  This
	// would be bad, so we need to make sure that we unregister them and
	// register the correct one.
	if (!__sync_bool_compare_and_swap(location, old, (id)HIDE_POINTER(value)))
	{
		return objc_assign_weak(value, location);
	}
	return value;
}

static SEL finalize;
static SEL cxx_destruct;

Class zombie_class;

struct objc_slot* objc_get_slot(Class cls, SEL selector);

static void runFinalize(void *addr, void *context)
{
	id obj = addr;
	//fprintf(stderr, "FINALIZING %p (%s)\n", addr, ((id)addr)->isa->name);
	if (Nil == ((id)addr)->isa) { return; }
	struct objc_slot *slot = objc_get_slot(obj->isa, cxx_destruct);
	if (NULL != slot)
	{
		slot->method(obj, cxx_destruct);
	}
	slot = objc_get_slot(obj->isa, finalize);
	if (NULL != slot)
	{
		slot->method(obj, finalize);
	}
	*(void**)addr = zombie_class;
}

static void collectIvarForClass(Class cls, GC_word *bitmap)
{
	for (unsigned i=0 ; (cls->ivars != 0) && (i<cls->ivars->count) ; i++)
	{
		struct objc_ivar *ivar = &cls->ivars->ivar_list[i];
		size_t start = ivar->offset;
		size_t end = i+1 < cls->ivars->count ? cls->ivars->ivar_list[i+1].offset
		                                     : cls->instance_size;
		switch (ivar->type[0])
		{
			case '[': case '{': case '(':
				// If the structure / array / union type doesn't contain any
				// pointers, then skip it.  We still need to be careful of packed
				if ((strchr(ivar->type, '^') == 0) &&
				    (strchr(ivar->type, '@') == 0))
				{
					break;
				}
			// Explicit pointer types
			case '^': case '@':
				for (unsigned b=(start / sizeof(void*)) ; b<(end/sizeof(void*)) ; b++)
				{
					GC_set_bit(bitmap, b);
				}
		}
	}
	if (cls->super_class)
	{
		collectIvarForClass(cls->super_class, bitmap);
	}
}

static GC_descr descriptor_for_class(Class cls)
{
	GC_descr descr = gc_typeForClass(cls);

	if (0 != descr) { return descr; }

	LOCK_RUNTIME_FOR_SCOPE();

	descr = (GC_descr)gc_typeForClass(cls);
	if (0 != descr) { return descr; }

	size_t size = cls->instance_size / 8 + 1;
	GC_word bitmap[size];
	memset(bitmap, 0, size);
	collectIvarForClass(cls, bitmap);
	// It's safe to round down here - if a class ends with an ivar that is
	// smaller than a pointer, then it can't possibly be a pointer.
	//fprintf(stderr, "Class is %d byes, %d words\n", cls->instance_size, cls->instance_size/sizeof(void*));
	descr = GC_make_descriptor(bitmap, cls->instance_size / sizeof(void*));
	gc_setTypeForClass(cls, descr);
	return descr;
}

static id allocate_class(Class cls, size_t extra)
{
	id obj = 0;
	// If there are some extra bytes, they may contain pointers, so we ignore
	// the type
	if (extra > 0)
	{
		// FIXME: Overflow checking!
		//obj = GC_malloc(class_getInstanceSize(cls) + extra);
		obj = GC_MALLOC(class_getInstanceSize(cls) + extra);
	}
	else
	{
		obj = GC_MALLOC_EXPLICITLY_TYPED(class_getInstanceSize(cls), 
			descriptor_for_class(cls));
	}
	//fprintf(stderr, "Allocating %p (%s + %d).  Base is %p\n", obj, cls->name, extra, GC_base(obj));
	// It would be nice not to register a finaliser if the object didn't
	// implement finalize or .cxx_destruct methods.  Unfortunately, this is not
	// possible, because a class may add a finalize method as it runs.
	GC_REGISTER_FINALIZER_NO_ORDER(obj, runFinalize, 0, 0, 0);
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
static struct gc_refcount
{
	/** Reference count */
	intptr_t refCount;
	/** Strong pointer */
	id ptr;
} null_refcount = {0};

static int refcount_compare(const void *ptr, struct gc_refcount rc)
{
	return ptr == rc.ptr;
}
static uint32_t ptr_hash(const void *ptr)
{
	// Bit-rotate right 4, since the lowest few bits in an object pointer will
	// always be 0, which is not so useful for a hash value
	return ((uintptr_t)ptr >> 4) | ((uintptr_t)ptr << ((sizeof(id) * 8) - 4));
}
static uint32_t refcount_hash(struct gc_refcount rc)
{
	return ptr_hash(rc.ptr);
}
static int isEmpty(struct gc_refcount rc)
{
	return rc.ptr == NULL;
}
#define MAP_TABLE_VALUE_NULL isEmpty
#define MAP_TABLE_NAME refcount
#define MAP_TABLE_COMPARE_FUNCTION refcount_compare
#define MAP_TABLE_HASH_KEY ptr_hash
#define MAP_TABLE_HASH_VALUE refcount_hash
#define MAP_TABLE_VALUE_TYPE struct gc_refcount
#define MAP_TABLE_VALUE_PLACEHOLDER null_refcount
#define MAP_TABLE_TYPES_BITMAP (1<<(offsetof(struct gc_refcount, ptr) / sizeof(void*)))
#define MAP_TABLE_ACCESS_BY_REFERENCE
#include "hash_table.h"

static refcount_table *refcounts;

id objc_gc_retain(id object)
{
	struct gc_refcount *refcount = refcount_table_get(refcounts, object);
	if (NULL == refcount)
	{
		LOCK_FOR_SCOPE(&(refcounts->lock));
		refcount = refcount_table_get(refcounts, object);
		if (NULL == refcount)
		{
			struct gc_refcount rc = { 1, object};
			refcount_insert(refcounts, rc);
			return object;
		}
	}
	__sync_fetch_and_add(&(refcount->refCount), 1);
	return object;
}
void objc_gc_release(id object)
{
	struct gc_refcount *refcount = refcount_table_get(refcounts, object);
	// This object has not been explicitly retained, don't release it
	if (0 == refcount) { return; }

	if (0 == __sync_sub_and_fetch(&(refcount->refCount), 1))
	{
		LOCK_FOR_SCOPE(&(refcounts->lock));
		refcount->ptr = 0;
		__sync_synchronize();
		// If another thread has incremented the reference count while we were
		// doing this, then we need to add the count back into the table,
		// otherwise we can carry on.
		if (!__sync_bool_compare_and_swap(&(refcount->refCount), 0, 0))
		{
			refcount->ptr = object;
		}
	}
}
int objc_gc_retain_count(id object)
{
	struct gc_refcount *refcount = refcount_table_get(refcounts, object);
	return (0 == refcount) ? 0 : refcount->refCount;
}


void* objc_gc_allocate_collectable(size_t size, BOOL isScanned)
{
	if ( isScanned)
	{
		return GC_MALLOC(size);
	}
	void *buf = GC_MALLOC_ATOMIC(size);
	memset(buf, 0, size);
	return buf;
}
void* objc_gc_reallocate_collectable(void *ptr, size_t size, BOOL isScanned)
{
	if (0 == size) { return 0; }

	void *new = isScanned ? GC_MALLOC(size) : GC_MALLOC_ATOMIC(size);

	if (0 == new) { return 0; }

	if (NULL != ptr)
	{
		size_t oldSize = GC_size(ptr);
		if (oldSize < size)
		{
			size = oldSize;
		}
		memcpy(new, ptr, size);
	}
	return new;
}

static void collectAndDumpStats(int signalNo)
{
	objc_collect(OBJC_EXHAUSTIVE_COLLECTION);
	GC_dump();
}

static void deferredFinalizer(void)
{
	GC_invoke_finalizers();
}

static void runFinalizers(void)
{
	if (finalizeThreaded)
	{
		dispatch_async_f(finalizer_queue, deferredFinalizer, NULL);
	}
	else
	{
		GC_invoke_finalizers();
	}
}

static void init(void)
{
	char *sigNumber;
	// Dump GC stats on exit - uncomment when debugging.
	if (getenv("LIBOBJC_DUMP_GC_STATUS_ON_EXIT"))
	{
		atexit(GC_dump);
	}
	if ((sigNumber = getenv("LIBOBJC_DUMP_GC_STATUS_ON_SIGNAL")))
	{
		int s = sigNumber[0] ? (int)strtol(sigNumber, NULL, 10) : SIGUSR2;
		signal(s, collectAndDumpStats);
	}
	refcounts = refcount_create(4096);
	GC_clear_roots();
	finalize = sel_registerName("finalize");
	cxx_destruct = sel_registerName(".cxx_destruct");
	GC_finalizer_notifier = runFinalizers;
}

BOOL objc_collecting_enabled(void)
{
	// Lock the GC in the current state once it's been queried.  This prevents
	// the loading of any modules with an incompatible GC mode.
	current_gc_mode = isGCEnabled ? GC_Required : GC_None;
	return isGCEnabled;
}

void objc_startCollectorThread(void)
{
	if (YES == finalizeThreaded) { return; }
	finalizer_queue = dispatch_queue_create("ObjC finalizeation thread", 0);
	finalizeThreaded = YES;
}

void objc_clear_stack(unsigned long options)
{
	// This isn't a very good implementation - we should really be working out
	// how much stack space is left somehow, but this is not possible to do
	// portably.
	int i[1024];
	int *addr = &i[0];
	memset(addr, 0, 1024);
	// Tell the compiler that something that it doesn't know about is touching
	// this memory, so it shouldn't optimise the allocation and memset away.
	__asm__  volatile ("" :  : "m"(addr) : "memory");

}
// FIXME: These are all stub implementations that should be replaced with
// something better
BOOL objc_is_finalized(void *ptr) { return NO; }
void objc_start_collector_thread(void) {}
void objc_finalizeOnMainThread(Class cls) {}

static void *debug_malloc(size_t s)
{
	return GC_MALLOC_UNCOLLECTABLE(s);
}
static void debug_free(void *ptr)
{
	GC_FREE(ptr);
}

PRIVATE struct gc_ops gc_ops_boehm =
{
	.allocate_class = allocate_class,
	.malloc         = debug_malloc,
	.free           = debug_free,
	.init           = init
};

PRIVATE void enableGC(BOOL exclude)
{
	isGCEnabled = YES;
	if (__sync_bool_compare_and_swap(&gc, &gc_ops_none, &gc_ops_boehm))
	{
		gc->init();
	}
}
