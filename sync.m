#include "objc/runtime.h"
#include "lock.h"

#include <stdio.h>
#include <stdlib.h>

int snprintf(char *restrict s, size_t n, const char *restrict format, ...);

@interface Fake
+ (void)dealloc;
@end

static mutex_t at_sync_init_lock;
static unsigned long long lockClassId;

void __objc_sync_init(void)
{
	INIT_LOCK(at_sync_init_lock);
}

IMP objc_msg_lookup(id, SEL);

static void deallocLockClass(id obj, SEL _cmd);

static inline Class findLockClass(id obj)
{
	struct objc_object object = { obj->isa };
	SEL dealloc = @selector(dealloc);
	// Find the first class where this lookup is correct
	if (objc_msg_lookup((id)&object, dealloc) != (IMP)deallocLockClass)
	{
		do {
			object.isa = class_getSuperclass(object.isa);
		} while (Nil != object.isa && 
				objc_msg_lookup((id)&object, dealloc) != (IMP)deallocLockClass);
	}
	if (Nil == object.isa) { return Nil; }
	// object->isa is now either the lock class, or a class which inherits from
	// the lock class
	Class lastClass;
	do {
		lastClass = object.isa;
		object.isa = class_getSuperclass(object.isa);
	} while (Nil != object.isa &&
		   objc_msg_lookup((id)&object, dealloc) == (IMP)deallocLockClass);
	return lastClass;
}

static inline Class initLockObject(id obj)
{
	Class lockClass; 
	if (class_isMetaClass(obj->isa))
	{
		lockClass = objc_allocateMetaClass(obj, sizeof(mutex_t));
	}
	else
	{
		char nameBuffer[40];
		snprintf(nameBuffer, 39, "hiddenlockClass%lld", lockClassId++);
		lockClass = objc_allocateClassPair(obj->isa, nameBuffer,
				sizeof(mutex_t));
	}
	const char *types =
		method_getTypeEncoding(class_getInstanceMethod(obj->isa,
					@selector(dealloc)));
	class_addMethod(lockClass, @selector(dealloc), (IMP)deallocLockClass,
			types);
	if (!class_isMetaClass(obj->isa))
	{
		objc_registerClassPair(lockClass);
	}

	mutex_t *lock = object_getIndexedIvars(lockClass);
	INIT_LOCK(*lock);

	obj->isa = lockClass;
	return lockClass;
}

static void deallocLockClass(id obj, SEL _cmd)
{
	Class lockClass = findLockClass(obj);
	Class realClass = class_getSuperclass(lockClass);
	// Free the lock
	mutex_t *lock = object_getIndexedIvars(lockClass);
	DESTROY_LOCK(lock);
	// Free the class
	objc_disposeClassPair(lockClass);
	// Reset the class then call the real -dealloc
	obj->isa = realClass;
	[obj dealloc];
}

// TODO: This should probably have a special case for classes conforming to the
// NSLocking protocol, just sending them a -lock message.
void objc_sync_enter(id obj)
{
	Class lockClass = findLockClass(obj);
	if (Nil == lockClass)
	{
		LOCK(&at_sync_init_lock);
		// Test again in case two threads call objc_sync_enter at once
		lockClass = findLockClass(obj);
		if (Nil == lockClass)
		{
			lockClass = initLockObject(obj);
		}
		UNLOCK(&at_sync_init_lock);
	}
	mutex_t *lock = object_getIndexedIvars(lockClass);
	LOCK(lock);
}

void objc_sync_exit(id obj)
{
	Class lockClass = findLockClass(obj);
	mutex_t *lock = object_getIndexedIvars(lockClass);
	LOCK(lock);
}
