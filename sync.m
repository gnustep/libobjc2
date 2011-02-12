#include "objc/runtime.h"
#include "lock.h"
#include "class.h"
#include "dtable.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef __clang__
#define SELECTOR(x) @selector(x)
#else
#define SELECTOR(x) (SEL)@selector(x)
#endif

int snprintf(char *restrict s, size_t n, const char *restrict format, ...);

@interface Fake
+ (void)dealloc;
@end

static mutex_t at_sync_init_lock;

void __objc_sync_init(void)
{
	INIT_LOCK(at_sync_init_lock);
}

IMP objc_msg_lookup(id, SEL);

static void deallocLockClass(id obj, SEL _cmd);

static inline Class findLockClass(id obj)
{
	struct objc_object object = { obj->isa };
	while (Nil != object.isa && 
	       !objc_test_class_flag(object.isa, objc_class_flag_lock_class))
	{
		object.isa = class_getSuperclass(object.isa);
	}
	return object.isa;
}

static Class allocateLockClass(Class superclass)
{
	Class newClass = calloc(1, sizeof(struct objc_class) + sizeof(mutex_t));

	if (Nil == newClass) { return Nil; }

	// Set up the new class
	newClass->isa = superclass->isa;
	// Set the superclass pointer to the name.  The runtime will fix this when
	// the class links are resolved.
	newClass->name = superclass->name;
	newClass->info = objc_class_flag_resolved | objc_class_flag_initialized |
		objc_class_flag_class | objc_class_flag_user_created |
		objc_class_flag_new_abi | objc_class_flag_hidden_class |
		objc_class_flag_lock_class;
	newClass->super_class = superclass;
	newClass->dtable = objc_copy_dtable_for_class(superclass->dtable, newClass);
	newClass->instance_size = superclass->instance_size;
	if (objc_test_class_flag(superclass, objc_class_flag_meta))
	{
		newClass->info |= objc_class_flag_meta;
	}
	mutex_t *lock = object_getIndexedIvars(newClass);
	INIT_LOCK(*lock);

	return newClass;
}

static inline Class initLockObject(id obj)
{
	Class lockClass = allocateLockClass(obj->isa); 
	if (class_isMetaClass(obj->isa))
	{
		obj->isa = lockClass;
	}
	else
	{
		const char *types =
			method_getTypeEncoding(class_getInstanceMethod(obj->isa,
						SELECTOR(dealloc)));
		class_addMethod(lockClass, SELECTOR(dealloc), (IMP)deallocLockClass,
				types);
		obj->isa = lockClass;
	}

	return lockClass;
}

static void deallocLockClass(id obj, SEL _cmd)
{
	Class lockClass = findLockClass(obj);
	Class realClass = class_getSuperclass(lockClass);
	// Call the real -dealloc method (this ordering is required in case the
	// user does @synchronized(self) in -dealloc)
	struct objc_super super = {obj, realClass};
	objc_msg_lookup_super(&super, SELECTOR(dealloc))(obj, SELECTOR(dealloc));
	// After calling [super dealloc], the object will no longer exist.
	// Free the lock
	mutex_t *lock = object_getIndexedIvars(lockClass);
	DESTROY_LOCK(lock);

	// FIXME: Low memory profile.
	SparseArrayDestroy(lockClass->dtable);

	// Free the class
	free(lockClass);
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
	UNLOCK(lock);
}
