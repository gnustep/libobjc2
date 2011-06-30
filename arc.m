#import "objc/runtime.h"
#import "objc/blocks_runtime.h"
#import "nsobject.h"
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
				//AutoreleasePool,
				SELECTOR(new));
		DeleteAutoreleasePool = class_getMethodImplementation(
				AutoreleasePool,
				SELECTOR(release));
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
	return [obj autorelease];
}

id objc_autoreleaseReturnValue(id obj)
{
#ifdef NO_PTHREADS
	return [obj autorelease];
#else
	id old = pthread_getspecific(ReturnRetained);
	objc_release(old);
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
	return old;
#endif
}

id objc_retain(id obj)
{
	return [obj retain];
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
	[obj release];
}

id objc_storeStrong(id *object, id value)
{
	value = [value retain];
	id oldValue = *object;
	*object = value;
	[oldValue release];
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
