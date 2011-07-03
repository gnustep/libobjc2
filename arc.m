#import "stdio.h"
#import "objc/runtime.h"
#import "objc/blocks_runtime.h"
#import "nsobject.h"
#import "class.h"
#import "selector.h"
#import "visibility.h"
#import "objc/hooks.h"
#import "objc/objc-arc.h"

#ifndef NO_PTHREADS
#include <pthread.h>
pthread_key_t ReturnRetained;
#endif


@interface NSAutoreleasePool
+ (Class)class;
+ (id)new;
- (void)release;
@end

static Class AutoreleasePool;
static IMP NewAutoreleasePool;
static IMP DeleteAutoreleasePool;
static IMP AutoreleaseAdd;

extern BOOL FastARCRetain;
extern BOOL FastARCRelease;
extern BOOL FastARCAutorelease;

static inline id retain(id obj)
{
	if (objc_test_class_flag(obj->isa, objc_class_flag_fast_arc))
	{
		intptr_t *refCount = ((intptr_t*)obj) - 1;
		__sync_fetch_and_add(refCount, 1);
		return obj;
	}
	return [obj retain];
}

static inline void release(id obj)
{
	if (objc_test_class_flag(obj->isa, objc_class_flag_fast_arc))
	{
		intptr_t *refCount = ((intptr_t*)obj) - 1;
		if (__sync_fetch_and_sub(refCount, 1) < 0)
		{
			objc_delete_weak_refs(obj);
			[obj dealloc];
		}
		return;
	}
	[obj release];
}

static inline id autorelease(id obj)
{
	if (objc_test_class_flag(obj->isa, objc_class_flag_fast_arc))
	{
		AutoreleaseAdd(AutoreleasePool, SELECTOR(addObject:), obj);
		return obj;
	}
	return [obj autorelease];
}


void *objc_autoreleasePoolPush(void)
{
	// TODO: This should be more lightweight.  We really just need to allocate
	// an array here...
	if (Nil == AutoreleasePool)
	{
		AutoreleasePool = objc_getRequiredClass("NSAutoreleasePool");
		[AutoreleasePool class];
		NewAutoreleasePool = class_getMethodImplementation(
				object_getClass(AutoreleasePool),
				SELECTOR(new));
		DeleteAutoreleasePool = class_getMethodImplementation(
				AutoreleasePool,
				SELECTOR(release));
		AutoreleaseAdd = class_getMethodImplementation(
				object_getClass(AutoreleasePool),
				SELECTOR(addObject:));
	}
	return NewAutoreleasePool(AutoreleasePool, SELECTOR(new));
}
void objc_autoreleasePoolPop(void *pool)
{
	// TODO: Keep a small pool of autorelease pools per thread and allocate
	// from there.
	DeleteAutoreleasePool(pool, SELECTOR(release));
}

id objc_autorelease(id obj)
{
	if (nil != obj)
	{
		obj = autorelease(obj);
	}
	return obj;
}

id objc_autoreleaseReturnValue(id obj)
{
#ifdef NO_PTHREADS
	return [obj autorelease];
#else
	id old = pthread_getspecific(ReturnRetained);
	objc_autorelease(old);
	pthread_setspecific(ReturnRetained, obj);
	return old;
#endif
}

id objc_retainAutoreleasedReturnValue(id obj)
{
#ifdef NO_PTHREADS
	return objc_retain(obj);
#else
	id old = pthread_getspecific(ReturnRetained);
	pthread_setspecific(ReturnRetained, NULL);
	// If the previous object was released  with objc_autoreleaseReturnValue()
	// just before return, then it will not have actually been autoreleased.
	// Instead, it will have been stored in TLS.  We just remove it from TLS
	// and undo the fake autorelease.
	//
	// If the object was not returned with objc_autoreleaseReturnValue() then
	// we actually autorelease the fake object. and then retain the argument.
	// In tis case, this is equivalent to objc_retain().
	if (obj != old)
	{
		objc_autorelease(old);
		objc_retain(obj);
	}
	return obj;
#endif
}

id objc_retain(id obj)
{
	if (nil == obj) { return nil; }
	return retain(obj);
}

id objc_retainAutorelease(id obj)
{
	return objc_autorelease(objc_retain(obj));
}

id objc_retainAutoreleaseReturnValue(id obj)
{
	return objc_autoreleaseReturnValue(objc_retain(obj));
}


id objc_retainBlock(id b)
{
	return _Block_copy(b);
}

void objc_release(id obj)
{
	if (nil == obj) { return; }
	release(obj);
}

id objc_storeStrong(id *addr, id value)
{
	value = objc_retain(value);
	id oldValue = *addr;
	*addr = value;
	objc_release(oldValue);
	return value;
}

////////////////////////////////////////////////////////////////////////////////
// Weak references
////////////////////////////////////////////////////////////////////////////////

typedef struct objc_weak_ref
{
	id obj;
	id *ref[4];
	struct objc_weak_ref *next;
} WeakRef;


static int weak_ref_compare(const id obj, const WeakRef weak_ref)
{
	return obj == weak_ref.obj;
}

static uint32_t ptr_hash(const void *ptr)
{
	// Bit-rotate right 4, since the lowest few bits in an object pointer will
	// always be 0, which is not so useful for a hash value
	return ((uintptr_t)ptr >> 4) | ((uintptr_t)ptr << ((sizeof(id) * 8) - 4));
}
static int weak_ref_hash(const WeakRef weak_ref)
{
	return ptr_hash(weak_ref.obj);
}
static int weak_ref_is_null(const WeakRef weak_ref)
{
	return weak_ref.obj == NULL;
}
const static WeakRef NullWeakRef;
#define MAP_TABLE_NAME weak_ref
#define MAP_TABLE_COMPARE_FUNCTION weak_ref_compare
#define MAP_TABLE_HASH_KEY ptr_hash
#define MAP_TABLE_HASH_VALUE weak_ref_hash
#define MAP_TABLE_HASH_VALUE weak_ref_hash
#define MAP_TABLE_VALUE_TYPE struct objc_weak_ref
#define MAP_TABLE_VALUE_NULL weak_ref_is_null
#define MAP_TABLE_VALUE_PLACEHOLDER NullWeakRef
#define MAP_TABLE_ACCESS_BY_REFERENCE 1
#define MAP_TABLE_SINGLE_THREAD 1
#define MAP_TABLE_NO_LOCK 1

#include "hash_table.h"

static weak_ref_table *weakRefs;
mutex_t weakRefLock;

PRIVATE void init_arc(void)
{
	weak_ref_initialize(&weakRefs, 128);
	INIT_LOCK(weakRefLock);
#ifndef NO_PTHREADS
	pthread_key_create(&ReturnRetained, (void(*)(void*))objc_release);
#endif
}

id objc_storeWeak(id *addr, id obj)
{
	id old = *addr;
	LOCK_FOR_SCOPE(&weakRefLock);
	WeakRef *oldRef = weak_ref_table_get(weakRefs, old);
	while (NULL != oldRef)
	{
		for (int i=0 ; i<4 ; i++)
		{
			if (oldRef->ref[i] == addr)
			{
				oldRef->ref[i] = 0;
				oldRef = 0;
				break;
			}
		}
	}
	if (nil == obj)
	{
		*addr = obj;
		return nil;
	}
	obj = _objc_weak_load(obj);
	if (nil != obj)
	{
		WeakRef *ref = weak_ref_table_get(weakRefs, obj);
		while (NULL != ref)
		{
			for (int i=0 ; i<4 ; i++)
			{
				if (0 == ref->ref[i])
				{
					ref->ref[i] = addr;
					return obj;
				}
			}
			if (ref->next == NULL)
			{
				break;
			}
		}
		if (NULL != ref)
		{
			ref->next = calloc(sizeof(WeakRef), 1);
			ref->next->ref[0] = addr;
		}
		else
		{
			WeakRef newRef = {0};
			newRef.obj = obj;
			newRef.ref[0] = addr;
			weak_ref_insert(weakRefs, newRef);
		}
	}
	return obj;
}

static void zeroRefs(WeakRef *ref, BOOL shouldFree)
{
	if (NULL != ref->next)
	{
		zeroRefs(ref->next, YES);
	}
	for (int i=0 ; i<4 ; i++)
	{
		if (0 != ref->ref[i])
		{
			*ref->ref[i] = 0;
		}
	}
	if (shouldFree)
	{
		free(ref);
	}
	else
	{
		memset(ref, 0, sizeof(WeakRef));
	}
}

void objc_delete_weak_refs(id obj)
{
	LOCK_FOR_SCOPE(&weakRefLock);
	WeakRef *oldRef = weak_ref_table_get(weakRefs, obj);
	if (0 != oldRef)
	{
		zeroRefs(oldRef, NO);
	}
}

id objc_loadWeakRetained(id* obj)
{
	LOCK_FOR_SCOPE(&weakRefLock);
	return objc_retain(*obj);
}

id objc_loadWeak(id* object)
{
	return objc_autorelease(objc_loadWeakRetained(object));
}

void objc_copyWeak(id *dest, id *src)
{
	objc_release(objc_initWeak(dest, objc_loadWeakRetained(src)));
}

void objc_moveWeak(id *dest, id *src)
{
	// FIXME: src can be zero'd here, removing the relationship between the
	// object and the pointer, which can be cheaper.
	objc_moveWeak(dest, src);
}

void objc_destroyWeak(id* obj)
{
	objc_storeWeak(obj, nil);
}

id objc_initWeak(id *object, id value)
{
	*object = nil;
	return objc_storeWeak(object, value);
}
