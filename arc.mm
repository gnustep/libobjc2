#define _LIBCPP_NO_EXCEPTIONS 1
#define TSL_NO_EXCEPTIONS 1
// Libc++ < 13 requires this for <vector> to be header only.  It is ignored in
// libc++ >= 14
#define _LIBCPP_DISABLE_EXTERN_TEMPLATE  1
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <atomic>
#include <type_traits>
#include <vector>
#include <tsl/robin_map.h>
#import "lock.h"
#import "objc/runtime.h"
#ifdef EMBEDDED_BLOCKS_RUNTIME
#import "objc/blocks_private.h"
#import "objc/blocks_runtime.h"
#else
#include <Block.h>
#include <Block_private.h>
#endif
#import "nsobject.h"
#import "class.h"
#import "selector.h"
#import "visibility.h"
#import "objc/hooks.h"
#import "objc/objc-arc.h"
#include "objc/message.h"

/**
 * Helper to send a manual message for retain / release.
 * We cannot use [object retain] and friends because recent clang will turn
 * that into a call to `objc_retain`, causing infinite recursion.
 */
#ifdef __GNUSTEP_MSGSEND__
#define ManualRetainReleaseMessage(object, selName, types) \
	((types)objc_msgSend)(object, @selector(selName))
#else
#define ManualRetainReleaseMessage(object, selName, types) \
	((types)(objc_msg_lookup(object, @selector(selName))))(object, @selector(selName))
#endif

extern "C" id (*_objc_weak_load)(id object);

#if defined(_WIN32)
// We're using the Fiber-Local Storage APIs on Windows
// because the TLS APIs won't pass app certification.
// Additionally, the FLS API surface is 1:1 mapped to
// the TLS API surface when fibers are not in use.
#	include "safewindows.h"
#	define arc_tls_store FlsSetValue
#	define arc_tls_load FlsGetValue
#	define TLS_CALLBACK(name) void WINAPI name

typedef DWORD arc_tls_key_t;
typedef void WINAPI(*arc_cleanup_function_t)(void*);
static inline arc_tls_key_t arc_tls_key_create(arc_cleanup_function_t cleanupFunction)
{
	return FlsAlloc(cleanupFunction);
}

#else // if defined(_WIN32)

#	ifndef NO_PTHREADS
#		include <pthread.h>
#		define arc_tls_store pthread_setspecific
#		define arc_tls_load pthread_getspecific
#		define TLS_CALLBACK(name) void name

typedef pthread_key_t arc_tls_key_t;
typedef void (*arc_cleanup_function_t)(void*);
static inline arc_tls_key_t arc_tls_key_create(arc_cleanup_function_t cleanupFunction)
{
	pthread_key_t key;
	pthread_key_create(&key, cleanupFunction);
	return key;
}
#	endif
#endif

#ifdef arc_tls_store
arc_tls_key_t ARCThreadKey;
#endif

#ifndef HAVE_BLOCK_USE_RR2
extern "C"
{
	extern struct objc_class _NSConcreteMallocBlock;
	extern struct objc_class _NSConcreteStackBlock;
	extern struct objc_class _NSConcreteGlobalBlock;
	extern struct objc_class _NSConcreteAutoBlock;
	extern struct objc_class _NSConcreteFinalizingBlock;
}
#endif

@interface NSAutoreleasePool
+ (Class)class;
+ (id)new;
- (void)release;
@end

#define POOL_SIZE (4096 / sizeof(void*) - (2 * sizeof(void*)))
/**
 * Structure used for ARC-managed autorelease pools.  This structure should be
 * exactly one page in size, so that it can be quickly allocated.  This does
 * not correspond directly to an autorelease pool.  The 'pool' returned by
 * objc_autoreleasePoolPush() may be an interior pointer to one of these
 * structures.
 */
struct arc_autorelease_pool
{
	/**
	 * Pointer to the previous autorelease pool structure in the chain.  Set
	 * when pushing a new structure on the stack, popped during cleanup.
	 */
	struct arc_autorelease_pool *previous;
	/**
	 * The current insert point.
	 */
	id *insert;
	/**
	 * The remainder of the page, an array of object pointers.  
	 */
	id pool[POOL_SIZE];
};

struct arc_tls
{
	struct arc_autorelease_pool *pool;
	id returnRetained;
};

/**
 * Type-safe wrapper around calloc.
 */
template<typename T>
static inline T* new_zeroed()
{
	return static_cast<T*>(calloc(1, sizeof(T)));
}

static inline struct arc_tls* getARCThreadData(void)
{
#ifndef arc_tls_store
	return NULL;
#else // !defined arc_tls_store
	auto tls = static_cast<struct arc_tls*>(arc_tls_load(ARCThreadKey));
	if (NULL == tls)
	{
		tls = new_zeroed<struct arc_tls>();
		arc_tls_store(ARCThreadKey, tls);
	}
	return tls;
#endif
}
static inline void release(id obj);

/**
 * Empties objects from the autorelease pool, stating at the head of the list
 * specified by pool and continuing until it reaches the stop point.  If the stop
 * point is NULL then all pools are cleared.
 */
static void emptyPool(struct arc_tls *tls, void *stopAt)
{
	/* Clear all pools by default. */
	struct arc_autorelease_pool *stopPool = NULL;
	void *oldPool;

	/* Are we clearing up to a given object? */
	if (stopAt != NULL)
	{
		stopPool = tls->pool;

		/* Find pool in which object to stop at is located. */
		while (stopPool != NULL)
		{
			if (stopAt >= (void *)stopPool->pool &&
			    stopAt < (void *)&stopPool->pool[POOL_SIZE])
			{
				break;
			}

			stopPool = stopPool->previous;
		}

		/* Invalid pointer, quit. */
		if (stopPool == NULL)
		{
			return;
		}
	}

	do
	{
		/* Clear all pools up to the stop pool. */
		while (tls->pool != stopPool)
		{
			while (tls->pool->insert > tls->pool->pool)
			{
				--tls->pool->insert;
				release(*tls->pool->insert);
			}

			oldPool = tls->pool;
			tls->pool = tls->pool->previous;
			free(oldPool);
		}

		/* If we cleared them all, quit. */
		if (tls->pool == NULL)
		{
			return;
		}

		/*
		 * Release objects down to the stopping point. If a new pool is
		 * pushed, never release below the pool's base.
		 */
		while (tls->pool->insert > (id *)stopAt &&
		       tls->pool->insert > tls->pool->pool)
		{
			--tls->pool->insert;
			release(*tls->pool->insert);
		}

	/* Be sure that releasing objects did not push any new pools. */
	} while (tls->pool != stopPool);
	/* fprintf(stderr, "New insert: %p.  Stop: %p\n", tls->pool->insert, stop); */
}

#ifdef arc_tls_store
static TLS_CALLBACK(cleanupPools)(struct arc_tls* tls)
{
	if (tls->returnRetained)
	{
		release(tls->returnRetained);
		tls->returnRetained = nil;
	}
	if (NULL != tls->pool)
	{
		emptyPool(tls, NULL);
		assert(NULL == tls->pool);
	}
	if (tls->returnRetained)
	{
		cleanupPools(tls);
	}
	free(tls);
}
#endif


static Class AutoreleasePool;
template<typename Return, typename... Arguments>
using Selector = Return (*)(id, SEL, Arguments...);
static Selector<id> NewAutoreleasePool;
static Selector<void> DeleteAutoreleasePool;
static Selector<void, id> AutoreleaseAdd;

static BOOL useARCAutoreleasePool;

namespace {
/**
 * The reference count that precedes every fast-ARC object.  It owns the bit
 * layout -- the weak flag, the guard bit, the count field and the deallocating
 * sentinel -- and all of the atomic manipulation of the count word, so that the
 * retain / release / weak entry points are expressed as operations rather than
 * as open-coded masks.  Obtain the reference count for an object with
 * `RefCount::fromObject(obj)`.  Keeping this behind a single type is also what
 * would let the count move into spare isa bits (with overflow spilling to a
 * side table) without touching any of the callers.
 */
class RefCount
{
	std::atomic<uintptr_t> *word;

	/**
	 * The top bit records whether the object has ever had a weak reference
	 * taken, which lets most objects skip the weak-table lock on deallocation.
	 */
	static const uintptr_t weak_flag =
		((uintptr_t)1) << ((sizeof(uintptr_t) * 8) - 1);
	/**
	 * The bit immediately below the weak flag is a guard.  It is never part of
	 * the logical count, so an optimistic `fetch_add` in the strong-retain fast
	 * path can carry a maxed-out count into it without ever reaching (and
	 * corrupting) the weak flag above it.
	 */
	static const uintptr_t guard = weak_flag >> 1;
	/** Every bit other than the weak flag and the guard is the count itself. */
	static const uintptr_t count_mask = ~(weak_flag | guard);
	/** The largest representable count; incrementing past it saturates. */
	static const uintptr_t count_max = count_mask - 1;
	/*
	 * A count field of all ones (== count_mask) is the deallocating sentinel,
	 * reached when the last reference (a stored count of zero) is decremented
	 * and the subtraction borrows.
	 */

	explicit RefCount(std::atomic<uintptr_t> *w) : word(w) {}

public:
	/** The count word sits immediately before the object. */
	static RefCount fromObject(id obj)
	{
		return RefCount(reinterpret_cast<std::atomic<uintptr_t>*>(obj) - 1);
	}

	/** The logical retain count (stored count + 1), or 0 while deallocating. */
	size_t retainCount() const
	{
		uintptr_t v = word->load(std::memory_order_relaxed);
		uintptr_t count = v & count_mask;
		return count == count_mask ? 0 : count + 1;
	}

	/** Whether the object has entered deallocation. */
	bool isDeallocating() const
	{
		return (word->load(std::memory_order_relaxed) & count_mask) == count_mask;
	}

	/**
	 * Strong retain.  The caller already owns a reference, so the object cannot
	 * be at (or reach) the deallocating sentinel while this runs: the final
	 * release only happens once every strong reference, including the caller's,
	 * is gone.  A single `fetch_add` is therefore safe, and the guard bit above
	 * the count means even a saturating increment cannot carry into the weak
	 * flag.  This replaces a compare-exchange retry loop, which re-spins on
	 * every lost race under contention.
	 */
	void increment()
	{
		uintptr_t old = word->fetch_add(1, std::memory_order_acq_rel);
		if (UNLIKELY((old & count_mask) >= count_max))
		{
			// Saturated (unreachable for any real object: it needs 2^62 live
			// references).  Undo the speculative increment and leave the count
			// pinned; the guard bit guaranteed the weak flag was untouched.
			word->fetch_sub(1, std::memory_order_relaxed);
		}
	}

	/**
	 * Weak-to-strong retain.  This can race a concurrent final release, so it
	 * must atomically check-and-increment (a `fetch_add` could resurrect an
	 * object that is already deallocating), which is why it keeps the
	 * compare-exchange loop.  Returns false if the object is already
	 * deallocating and must not be retained.
	 *
	 * The deallocating case arises when one thread acquires a strong reference
	 * from a weak reference while another destroys the object: the deallocating
	 * thread decrements the count with no lock held, then takes the weak-ref
	 * table lock to zero the weak references, while `objc_loadWeakRetained`
	 * (this path's caller) also holds that lock.  If the decrement is serialised
	 * before this increment we return false so the object is actually
	 * destroyed; if it is serialised after, the deallocating thread's locked
	 * count check sees our reference and skips the destruction.
	 */
	bool incrementIfLive()
	{
		uintptr_t v = word->load(std::memory_order_relaxed);
		for (;;)
		{
			uintptr_t count = v & count_mask;
			if (count == count_mask)
			{
				// Already deallocating: fail the weak-to-strong transition.
				return false;
			}
			if (count == count_max)
			{
				// Saturated: leave the count pinned.
				return true;
			}
			uintptr_t updated = (count + 1) | (v & weak_flag);
			// Acquire/release on the exchange so reference-count updates are
			// ordered against each other on weakly-ordered targets.  On a failed
			// exchange `v` is refreshed with the current value.
			if (word->compare_exchange_weak(v, updated,
			                                std::memory_order_acq_rel,
			                                std::memory_order_acquire))
			{
				return true;
			}
		}
	}

	/**
	 * Drop one reference.  Returns true if this dropped the last reference (the
	 * object should now be destroyed), setting `wasWeaklyReferenced` to whether
	 * the object had ever had a weak reference taken.  Release ordering on the
	 * decrement makes writes through the dropped references visible to whichever
	 * thread performs the final release.
	 */
	bool decrement(bool &wasWeaklyReferenced)
	{
		uintptr_t old = word->fetch_sub(1, std::memory_order_release);
		uintptr_t count = old & count_mask;
		if (LIKELY((count != 0) && (count < count_max)))
		{
			// The common case: a live object with other references remaining.
			return false;
		}
		if (count >= count_max)
		{
			// Saturated, or already at the deallocating sentinel from an
			// over-release: the decrement must not stand, so undo it.  The guard
			// bit keeps the undo off the weak flag.
			word->fetch_add(1, std::memory_order_relaxed);
			return false;
		}
		// count == 0: this dropped the last reference.  The borrow leaves the
		// count field at the deallocating sentinel, which every other path keys
		// off; it also flips the weak flag, but that is never observed (the
		// sentinel is what -objc_delete_weak_refs and -setObjectHasWeakRefs test,
		// the weak state is taken from the pre-decrement value here, and the word
		// is freed immediately afterwards).
		std::atomic_thread_fence(std::memory_order_acquire);
		wasWeaklyReferenced = (old & weak_flag) == weak_flag;
		return true;
	}

	/**
	 * Record that the object has a weak reference.  The flag is monotonic (set,
	 * never cleared), so this is a no-op once it is set, and it is a no-op once
	 * the object is deallocating.
	 */
	void markWeaklyReferenced()
	{
		uintptr_t v = word->load(std::memory_order_relaxed);
		for (;;)
		{
			uintptr_t count = v & count_mask;
			if (count == count_mask)
			{
				// Deallocating (or deallocated): nothing to record.
				return;
			}
			if ((v & weak_flag) == weak_flag)
			{
				// Already set; monotonic, so don't try to re-set it.
				return;
			}
			uintptr_t updated = count | weak_flag;
			if (word->compare_exchange_weak(v, updated,
			                                std::memory_order_acq_rel,
			                                std::memory_order_acquire))
			{
				return;
			}
		}
	}
};
} // namespace

extern "C" OBJC_PUBLIC size_t object_getRetainCount_np(id obj)
{
	return RefCount::fromObject(obj).retainCount();
}

static id retain_fast(id obj, BOOL isWeak)
{
	RefCount refCount = RefCount::fromObject(obj);
	if (LIKELY(!isWeak))
	{
		refCount.increment();
		return obj;
	}
	return refCount.incrementIfLive() ? obj : nil;
}

extern "C" OBJC_PUBLIC id objc_retain_fast_np(id obj)
{
	return retain_fast(obj, NO);
}

__attribute__((always_inline))
static inline BOOL isPersistentObject(id obj)
{
	// No reference count manipulations on nil objects.
	if (obj == nil)
	{
		return YES;
	}
	// Small objects are never accessibly by reference
	if (isSmallObject(obj))
	{
		return YES;
	}
	// Persistent objects are persistent.  Safe to access isa directly here
	// because we've already handled the small object case separately.
	return objc_test_class_flag(obj->isa, objc_class_flag_permanent_instances);
}

static inline id retain(id obj, BOOL isWeak)
{
	if (isPersistentObject(obj)) { return obj; }
	Class cls = obj->isa;
	if (UNLIKELY(objc_test_class_flag(cls, objc_class_flag_is_block)))
	{
		return Block_copy(obj);
	}
	if (objc_test_class_flag(cls, objc_class_flag_fast_arc))
	{
		return retain_fast(obj, isWeak);
	}
	return ManualRetainReleaseMessage(obj, retain, id(*)(id, SEL));
}

extern "C" OBJC_PUBLIC BOOL objc_release_fast_no_destroy_np(id obj)
{
	bool wasWeaklyReferenced;
	if (!RefCount::fromObject(obj).decrement(wasWeaklyReferenced))
	{
		return NO;
	}
	// This dropped the last reference, so the object is now deallocating.  Zero
	// any weak references to it before the caller destroys it.
	if (wasWeaklyReferenced && !objc_delete_weak_refs(obj))
	{
		return NO;
	}
	return YES;
}

extern "C" OBJC_PUBLIC void objc_release_fast_np(id obj)
{
	if (objc_release_fast_no_destroy_np(obj))
	{
		[obj dealloc];
	}
}

static inline void release(id obj)
{
	if (isPersistentObject(obj)) { return; }
	Class cls = obj->isa;
	if (UNLIKELY(objc_test_class_flag(cls, objc_class_flag_is_block)))
	{
		if (cls == static_cast<void*>(&_NSConcreteStackBlock))
		{
			return;
		}
		_Block_release(obj);
		return;
	}
	if (objc_test_class_flag(cls, objc_class_flag_fast_arc))
	{
		objc_release_fast_np(obj);
		return;
	}
	return ManualRetainReleaseMessage(obj, release, void(*)(id, SEL));
}

static inline void initAutorelease(void)
{
	if (Nil == AutoreleasePool)
	{
		AutoreleasePool = objc_getClass("NSAutoreleasePool");
		if (Nil == AutoreleasePool)
		{
			useARCAutoreleasePool = YES;
		}
		else
		{
			useARCAutoreleasePool = (0 != class_getInstanceMethod(AutoreleasePool,
			                                                      SELECTOR(_ARCCompatibleAutoreleasePool)));
			if (!useARCAutoreleasePool)
			{
				[AutoreleasePool class];
				auto storeSelector = [](auto &target, Class cls, SEL selector)
				{
					target = reinterpret_cast<std::remove_reference_t<decltype(target)>>(
						class_getMethodImplementation(cls, selector));
				};
				storeSelector(NewAutoreleasePool, object_getClass(AutoreleasePool), SELECTOR(new));
				storeSelector(DeleteAutoreleasePool, AutoreleasePool, SELECTOR(release));
				storeSelector(AutoreleaseAdd, object_getClass(AutoreleasePool), SELECTOR(addObject:));
			}
		}
	}
}

static inline id autorelease(id obj)
{
	//fprintf(stderr, "Autoreleasing %p\n", obj);
	if (useARCAutoreleasePool)
	{
		struct arc_tls *tls = getARCThreadData();
		if (NULL != tls)
		{
			struct arc_autorelease_pool *pool = tls->pool;
			if (NULL == pool || (pool->insert >= &pool->pool[POOL_SIZE]))
			{
				pool = new_zeroed<struct arc_autorelease_pool>();
				pool->previous = tls->pool;
				pool->insert = pool->pool;
				tls->pool = pool;
			}
			*pool->insert = obj;
			pool->insert++;
			return obj;
		}
	}
	if (objc_test_class_flag(classForObject(obj), objc_class_flag_fast_arc))
	{
		initAutorelease();
		if (0 != AutoreleaseAdd)
		{
			AutoreleaseAdd(AutoreleasePool, SELECTOR(addObject:), obj);
		}
		return obj;
	}
	return ManualRetainReleaseMessage(obj, autorelease, id(*)(id, SEL));
}

extern "C" OBJC_PUBLIC unsigned long objc_arc_autorelease_count_np(void)
{
	struct arc_tls* tls = getARCThreadData();
	unsigned long count = 0;
	if (!tls) { return 0; }

	for (struct arc_autorelease_pool *pool=tls->pool ;
	     NULL != pool ;
	     pool = pool->previous)
	{
		count += (((intptr_t)pool->insert) - ((intptr_t)pool->pool)) / sizeof(id);
	}
	return count;
}
extern "C" OBJC_PUBLIC unsigned long objc_arc_autorelease_count_for_object_np(id obj)
{
	struct arc_tls* tls = getARCThreadData();
	unsigned long count = 0;
	if (!tls) { return 0; }

	for (struct arc_autorelease_pool *pool=tls->pool ;
	     NULL != pool ;
	     pool = pool->previous)
	{
		for (id* o = pool->insert-1 ; o >= pool->pool ; o--)
		{
			if (*o == obj)
			{
				count++;
			}
		}
	}
	return count;
}

extern "C" OBJC_PUBLIC void *objc_autoreleasePoolPush(void)
{
	initAutorelease();
	struct arc_tls* tls = getARCThreadData();
	// If there is an object in the return-retained slot, then we need to
	// promote it to the real autorelease pool BEFORE pushing the new
	// autorelease pool.  If we don't, then it may be prematurely autoreleased.
	if ((NULL != tls) && (nil != tls->returnRetained))
	{
		autorelease(tls->returnRetained);
		tls->returnRetained = nil;
	}
	if (useARCAutoreleasePool)
	{
		if (NULL != tls)
		{
			struct arc_autorelease_pool *pool = tls->pool;
			if (NULL == pool || (pool->insert >= &pool->pool[POOL_SIZE]))
			{
				pool = new_zeroed<struct arc_autorelease_pool>();
				pool->previous = tls->pool;
				pool->insert = pool->pool;
				tls->pool = pool;
			}
			// If there is no autorelease pool allocated for this thread, then
			// we lazily allocate one the first time something is autoreleased.
			return (NULL != tls->pool) ? tls->pool->insert : NULL;
		}
	}
	initAutorelease();
	if (0 == NewAutoreleasePool) { return NULL; }
	return NewAutoreleasePool(AutoreleasePool, SELECTOR(new));
}
extern "C" OBJC_PUBLIC void objc_autoreleasePoolPop(void *pool)
{
	if (useARCAutoreleasePool)
	{
		struct arc_tls* tls = getARCThreadData();
		if (NULL != tls)
		{
			if (NULL != tls->pool)
			{
				emptyPool(tls, pool);
			}
			return;
		}
	}
	DeleteAutoreleasePool(static_cast<id>(pool), SELECTOR(release));
	struct arc_tls* tls = getARCThreadData();
	if (tls && tls->returnRetained)
	{
		release(tls->returnRetained);
		tls->returnRetained = nil;
	}
}

extern "C" OBJC_PUBLIC id objc_autorelease(id obj)
{
	if (nil != obj)
	{
		obj = autorelease(obj);
	}
	return obj;
}

extern "C" OBJC_PUBLIC id objc_autoreleaseReturnValue(id obj)
{
	if (!useARCAutoreleasePool) 
	{
		struct arc_tls* tls = getARCThreadData();
		if (NULL != tls)
		{
			objc_autorelease(tls->returnRetained);
			tls->returnRetained = obj;
			return obj;
		}
	}
	return objc_autorelease(obj);
}

extern "C" OBJC_PUBLIC id objc_retainAutoreleasedReturnValue(id obj)
{
	// If the previous object was released  with objc_autoreleaseReturnValue()
	// just before return, then it will not have actually been autoreleased.
	// Instead, it will have been stored in TLS.  We just remove it from TLS
	// and undo the fake autorelease.
	//
	// If the object was not returned with objc_autoreleaseReturnValue() then
	// we actually autorelease the fake object. and then retain the argument.
	// In tis case, this is equivalent to objc_retain().
	struct arc_tls* tls = getARCThreadData();
	if (NULL != tls)
	{
		// If we're using our own autorelease pool, just pop the object from the top
		if (useARCAutoreleasePool)
		{
			if ((NULL != tls->pool) &&
			    (*(tls->pool->insert-1) == obj))
			{
				tls->pool->insert--;
				return obj;
			}
		}
		else if (obj == tls->returnRetained)
		{
			tls->returnRetained = NULL;
			return obj;
		}
	}
	return objc_retain(obj);
}

extern "C" OBJC_PUBLIC id objc_retain(id obj)
{
	if (nil == obj) { return nil; }
	return retain(obj, NO);
}

extern "C" OBJC_PUBLIC id objc_retainAutorelease(id obj)
{
	return objc_autorelease(objc_retain(obj));
}

extern "C" OBJC_PUBLIC id objc_retainAutoreleaseReturnValue(id obj)
{
	if (nil == obj) { return obj; }
	return objc_autoreleaseReturnValue(retain(obj, NO));
}


extern "C" OBJC_PUBLIC id objc_retainBlock(id b)
{
	return static_cast<id>(_Block_copy(b));
}

extern "C" OBJC_PUBLIC void objc_release(id obj)
{
	if (nil == obj) { return; }
	release(obj);
}

extern "C" OBJC_PUBLIC void objc_storeStrong(id *addr, id value)
{
	value = objc_retain(value);
	id oldValue = *addr;
	*addr = value;
	objc_release(oldValue);
}

////////////////////////////////////////////////////////////////////////////////
// Weak references
////////////////////////////////////////////////////////////////////////////////

static int weakref_class;

namespace {

// Weak-reference control block.  `shardIndex` is the owning stripe: set once,
// never changed, and blocks are recycled (never freed), so a slot-first
// operation can read it to pick the stripe without a lock.
struct WeakRef
{
	void *isa;
	id obj = nullptr;
	size_t weak_count = 1;
	size_t shardIndex;
	WeakRef *nextFree = nullptr;  // valid only while on a stripe's free list
	WeakRef(id o, size_t shard) : obj(o), shardIndex(shard)
	{
		// isa is read without a lock by asWeakRef, so publish it atomically.
		__atomic_store_n(&isa, (void*)&weakref_class, __ATOMIC_RELAXED);
	}
};

template<typename T>
struct malloc_allocator
{
	typedef T value_type;
	T* allocate(std::size_t n)
	{
		return static_cast<T*>(malloc(sizeof(T) * n));
	}

	void deallocate(T* p, std::size_t)
	{
		free(p);
	}

	template<typename X>
	malloc_allocator &operator=(const malloc_allocator<X>&) const
	{
		return *this;
	}

	bool operator==(const malloc_allocator &) const
	{
		return true;
	}

	template<typename X>
	operator malloc_allocator<X>() const
	{
		return malloc_allocator<X>();
	}
};

using weak_ref_map = tsl::robin_pg_map<const void*,
                                       WeakRef*,
                                       std::hash<const void*>,
                                       std::equal_to<const void*>,
                                       malloc_allocator<std::pair<const void*, WeakRef*>>>;

// A weak slot is accessed under whichever stripe lock owns the block it points
// at, so no single lock serialises it: use atomic acquire/release.
static inline id weakSlotLoad(id *slot)
{
	return (id)__atomic_load_n((void**)slot, __ATOMIC_ACQUIRE);
}
static inline void weakSlotStore(id *slot, id value)
{
	__atomic_store_n((void**)slot, (void*)value, __ATOMIC_RELEASE);
}

// If `p` is a control block, return it so the caller can pick the owning stripe
// before locking; otherwise (nil, tagged pointer, real object) return nullptr.
// isa is read atomically as a concurrent recycle may republish it.
static inline WeakRef *asWeakRef(id p)
{
	if ((p == nil) || isSmallObject(p))
	{
		return nullptr;
	}
	if (__atomic_load_n((void**)&p->isa, __ATOMIC_RELAXED) == (void*)&weakref_class)
	{
		return reinterpret_cast<WeakRef*>(p);
	}
	return nullptr;
}

// Sharded weak-reference table: the striping is internal; callers work in terms
// of objects and slots.  NumShards is a compile-time power of two.  Every
// operation runs inside withSlotLocked / withStoreLocked, which hold the owning
// stripe lock(s); the helpers below assume that lock is held.
template<size_t NumShards>
class WeakRefTable
{
	static_assert((NumShards & (NumShards - 1)) == 0,
	              "NumShards must be a power of two");

	// One stripe: lock, map and free list.  Cache-line aligned so stripes do not
	// false-share.  The free list recycles blocks (never freed) so their memory
	// and immutable shardIndex stay valid for lock-free selection; it is guarded
	// by the stripe lock the callers already hold.
	struct alignas(64) Shard
	{
		mutex_t lock;
		weak_ref_map map;
		WeakRef *freeList = nullptr;
		Shard() : map(16) { INIT_LOCK(lock); }
	};
	Shard shards[NumShards];

	// Object address -> stripe.  The low bits are alignment, so fold in higher
	// bits before masking.
	static inline size_t indexFor(const void *obj)
	{
		uintptr_t a = reinterpret_cast<uintptr_t>(obj);
		return ((a >> 4) ^ (a >> 12) ^ (a >> 20)) & (NumShards - 1);
	}

public:
	static const size_t NONE = ~static_cast<size_t>(0);  // "no stripe" for Guard

	// Locks one or two stripes (NONE = none) in ascending index order,
	// de-duplicating.  The ordering makes two-object operations deadlock-free.
	class Guard
	{
		Shard *s0 = nullptr;
		Shard *s1 = nullptr;
	public:
		Guard(WeakRefTable &t, size_t i, size_t j = NONE)
		{
			size_t a = i, b = j;
			if ((a != NONE) && (b != NONE))
			{
				if (a == b) { b = NONE; }
				else if (a > b) { size_t x = a; a = b; b = x; }
			}
			else if (a == NONE) { a = b; b = NONE; }
			if (a != NONE) { s0 = &t.shards[a]; LOCK(&s0->lock); }
			if (b != NONE) { s1 = &t.shards[b]; LOCK(&s1->lock); }
		}
		Guard(const Guard&) = delete;
		Guard &operator=(const Guard&) = delete;
		~Guard()
		{
			if (s1) { UNLOCK(&s1->lock); }
			if (s0) { UNLOCK(&s0->lock); }
		}
	};

	// Construct the stripes (and init their locks) before first use.
	void init() { (void)shards[0].map.size(); }

	// Run fn(ref, raw) with the stripe owning the slot's current weak reference
	// locked (ref == nullptr, no lock, for a nil/strong slot).  The slot may be
	// repointed between the lock-free peek and the lock; re-check and retry.  fn
	// uses `raw`, never a fresh load, so it cannot return a value that appeared
	// after the check.
	template<typename Fn>
	auto withSlotLocked(id *slot, Fn &&fn) -> decltype(fn((WeakRef*)nullptr, (id)nil))
	{
		for (;;)
		{
			id raw = weakSlotLoad(slot);
			WeakRef *peek = asWeakRef(raw);
			Guard g(*this, peek ? peek->shardIndex : NONE);
			if (weakSlotLoad(slot) != raw)
			{
				continue;
			}
			return fn(peek, raw);
		}
	}

	// As withSlotLocked, but also locks the stripe owning `newObj` (storeWeak
	// touches the slot's current target and the new object).
	template<typename Fn>
	auto withStoreLocked(id *slot, id newObj, Fn &&fn) -> decltype(fn((WeakRef*)nullptr, (id)nil))
	{
		size_t sNew = newObj ? indexFor(newObj) : NONE;
		for (;;)
		{
			id raw = weakSlotLoad(slot);
			WeakRef *peek = asWeakRef(raw);
			Guard g(*this, peek ? peek->shardIndex : NONE, sNew);
			if (weakSlotLoad(slot) != raw)
			{
				continue;
			}
			return fn(peek, raw);
		}
	}

	size_t shardOf(id obj) { return indexFor(obj); }  // stripe index owning `obj`

	// Map of the stripe owning `obj` (caller holds its lock).
	weak_ref_map &mapFor(id obj) { return shards[indexFor(obj)].map; }

	// Control block for `obj`, incrementing its weak count.  Caller holds the lock.
	WeakRef *increment(id obj)
	{
		size_t shard = indexFor(obj);
		WeakRef *&ref = shards[shard].map[obj];
		if (ref == nullptr)
		{
			ref = alloc(obj, shard);
		}
		else
		{
			assert(ref->obj == obj);
			ref->weak_count++;
		}
		return ref;
	}

	// Drop one weak reference; recycle the block when the last goes.  Caller
	// holds the lock.
	BOOL release(WeakRef *ref)
	{
		ref->weak_count--;
		if (ref->weak_count == 0)
		{
			shards[ref->shardIndex].map.erase(ref->obj);
			recycle(ref);
			return YES;
		}
		return NO;
	}

private:
	// A block for `obj` in `shard`, recycled if one is available.
	WeakRef *alloc(id obj, size_t shard)
	{
		Shard &s = shards[shard];
		WeakRef *ref = s.freeList;
		if (ref != nullptr)
		{
			s.freeList = ref->nextFree;
			__atomic_store_n(&ref->isa, (void*)&weakref_class, __ATOMIC_RELAXED);
			ref->obj = obj;
			ref->weak_count = 1;
			ref->nextFree = nullptr;
			// shardIndex is already == shard and never changes.
		}
		else
		{
			ref = new WeakRef(obj, shard);
		}
		return ref;
	}

	// Return a block to its stripe's free list, keeping its memory valid.
	void recycle(WeakRef *ref)
	{
		Shard &s = shards[ref->shardIndex];
		ref->obj = nil;
		ref->nextFree = s.freeList;
		s.freeList = ref;
	}
};

#ifndef OBJC_WEAK_SHARD_COUNT
#define OBJC_WEAK_SHARD_COUNT 64
#endif

using weak_table_t = WeakRefTable<OBJC_WEAK_SHARD_COUNT>;

// The function-local static constructs the stripes (and inits their locks)
// before any weak operation can run.
static inline weak_table_t &weakTable()
{
	static weak_table_t t;
	return t;
}

}

#ifdef HAVE_BLOCK_USE_RR2
static const struct Block_callbacks_RR blocks_runtime_callbacks = {
		sizeof(Block_callbacks_RR),
		(void (*)(const void*))objc_retain,
		(void (*)(const void*))objc_release,
		(void (*)(const void*))objc_delete_weak_refs
	};
#endif

PRIVATE extern "C" void init_arc(void)
{
	// Force the weak-table stripes (and their locks) to be constructed before
	// any weak operation can run.
	weakTable().init();
#ifdef arc_tls_store
	ARCThreadKey = arc_tls_key_create((arc_cleanup_function_t)cleanupPools);
#endif
#ifdef HAVE_BLOCK_USE_RR2
	_Block_use_RR2(&blocks_runtime_callbacks);
#endif
}

extern "C" void* block_load_weak(void *block);

static BOOL setObjectHasWeakRefs(id obj)
{
	BOOL isGlobalObject = isPersistentObject(obj);
	Class cls = isGlobalObject ? Nil : obj->isa;
	if (obj && cls && objc_test_class_flag(cls, objc_class_flag_fast_arc))
	{
		// We hold the owning stripe lock, so a thread racing to deallocate waits
		// if we win the update.
		RefCount::fromObject(obj).markWeaklyReferenced();
	}
	return isGlobalObject;
}

extern "C" OBJC_PUBLIC id objc_storeWeak(id *addr, id obj)
{
	auto &t = weakTable();
	return t.withStoreLocked(addr, obj, [&](WeakRef *oldRef, id raw) -> id {
		// Both stripe locks are held, so oldRef, oldRef->obj and the slot are stable.
		id old = oldRef ? oldRef->obj : raw;
		// If the old and new values are the same, then we don't need to do
		// anything unless we are deleting the weak reference by storing NULL.
		if ((old == obj) && ((obj != NULL) || (NULL == oldRef)))
		{
			return obj;
		}
		BOOL isGlobalObject = setObjectHasWeakRefs(obj);
		// If an old ref exists, decrement its reference count.  This may also
		// recycle the weak reference control block.
		if (oldRef != NULL)
		{
			t.release(oldRef);
		}
		// If we're storing nil, then just write a null pointer.
		if (nil == obj)
		{
			weakSlotStore(addr, obj);
			return nil;
		}
		if (isGlobalObject)
		{
			// A global object is never deallocated, so secretly make this a
			// strong reference.
			weakSlotStore(addr, obj);
			return obj;
		}
		Class cls = classForObject(obj);
		if (UNLIKELY(objc_test_class_flag(cls, objc_class_flag_is_block)))
		{
			// Check whether the block is being deallocated and return nil if so
			if (_Block_isDeallocating(obj))
			{
				weakSlotStore(addr, nil);
				return nil;
			}
		}
		else if (object_getRetainCount_np(obj) == 0)
		{
			// If the object is being deallocated return nil.
			weakSlotStore(addr, nil);
			return nil;
		}
		if (nil != obj)
		{
			weakSlotStore(addr, (id)t.increment(obj));
		}
		return obj;
	});
}

extern "C" OBJC_PUBLIC BOOL objc_delete_weak_refs(id obj)
{
	auto &t = weakTable();
	typename weak_table_t::Guard guard(t, t.shardOf(obj));
	if (objc_test_class_flag(classForObject(obj), objc_class_flag_fast_arc))
	{
		// Don't proceed if the object isn't deallocating.
		if (!RefCount::fromObject(obj).isDeallocating())
		{
			return NO;
		}
	}
	auto &table = t.mapFor(obj);
	auto old = table.find(obj);
	if (old != table.end())
	{
		WeakRef *oldRef = old->second;
		// The address of obj is likely to be reused, so remove it from the
		// table so that we don't accidentally alias weak references.
		table.erase(old);
		// Zero the object pointer.  This prevents any other weak accesses from
		// loading from this.  It must be done after removing the ref from the
		// table, because the compare operation tests the obj field.
		oldRef->obj = nil;
		// If the weak reference count is zero, then we should have already
		// removed this.
		assert(oldRef->weak_count > 0);
	}
	return YES;
}

extern "C" OBJC_PUBLIC id objc_loadWeakRetained(id* addr)
{
	auto &t = weakTable();
	return t.withSlotLocked(addr, [&](WeakRef *peek, id raw) -> id {
		// A nil or non-deallocatable strong value needs no table access.
		if (peek == nullptr)
		{
			return raw;
		}
		// The stripe lock is held, so peek and peek->obj are stable, and the
		// object cannot be deallocated (release acquires the lock first).
		id obj = peek->obj;
		if (obj == nil)
		{
			// The object is destroyed; drop this reference to the control block.
			t.release(peek);
			weakSlotStore(addr, nil);
			return nil;
		}
		Class cls = classForObject(obj);
		if (objc_test_class_flag(cls, objc_class_flag_permanent_instances))
		{
			return obj;
		}
		else if (UNLIKELY(objc_test_class_flag(cls, objc_class_flag_is_block)))
		{
			obj = static_cast<id>(block_load_weak(obj));
			if (obj == nil)
			{
				return nil;
			}
			// A defeasible retain that protects against another thread
			// concurrently starting to deallocate the block.
			if (_Block_tryRetain(obj))
			{
				return obj;
			}
			return nil;
		}
		else if (!objc_test_class_flag(cls, objc_class_flag_fast_arc))
		{
			obj = _objc_weak_load(obj);
		}
		// _objc_weak_load() can return nil
		if (obj == nil) { return nil; }
		return retain(obj, YES);
	});
}

extern "C" OBJC_PUBLIC id objc_loadWeak(id* object)
{
	return objc_autorelease(objc_loadWeakRetained(object));
}

extern "C" OBJC_PUBLIC void objc_copyWeak(id *dest, id *src)
{
	// Don't retain or release.
	// `src` is a valid pointer to a __weak pointer or nil.
	// `dest` is a valid pointer to uninitialised memory.
	// After this operation, `dest` should contain whatever `src` contained.
	weakTable().withSlotLocked(src, [&](WeakRef *peek, id raw) -> char {
		*dest = raw;
		if (peek)
		{
			peek->weak_count++;  // took another reference to the control block
		}
		return 0;
	});
}

extern "C" OBJC_PUBLIC void objc_moveWeak(id *dest, id *src)
{
	// Don't retain or release.
	// `dest` is a valid pointer to uninitialized memory.
	// `src` is a valid pointer to a __weak pointer.
	// This operation moves from *src to *dest and must be atomic with respect
	// to other stores to *src via `objc_storeWeak`; the stripe lock provides it.
	weakTable().withSlotLocked(src, [&](WeakRef *, id raw) -> char {
		*dest = raw;
		weakSlotStore(src, nil);
		return 0;
	});
}

extern "C" OBJC_PUBLIC void objc_destroyWeak(id* obj)
{
	weakTable().withSlotLocked(obj, [&](WeakRef *peek, id) -> char {
		// If the slot held a weak reference, decrement its count (may recycle it).
		if (peek != NULL)
		{
			weakTable().release(peek);
		}
		return 0;
	});
}

extern "C" OBJC_PUBLIC id objc_initWeak(id *addr, id obj)
{
	if (obj == nil)
	{
		weakSlotStore(addr, nil);
		return nil;
	}
	auto &t = weakTable();
	typename weak_table_t::Guard guard(t, t.shardOf(obj));
	BOOL isGlobalObject = setObjectHasWeakRefs(obj);
	if (isGlobalObject)
	{
		// A global object is never deallocated, so secretly make this a strong
		// reference.
		weakSlotStore(addr, obj);
		return obj;
	}
	// If the object is being deallocated return nil.
	if (object_getRetainCount_np(obj) == 0)
	{
		weakSlotStore(addr, nil);
		return nil;
	}
	if (nil != obj)
	{
		weakSlotStore(addr, (id)t.increment(obj));
	}
	return obj;
}
