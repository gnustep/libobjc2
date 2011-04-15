#include "objc/runtime.h"
#include "lock.h"
#include <stdio.h>
#include <stdlib.h>

int snprintf(char *restrict s, size_t n, const char *restrict format, ...);

@interface __ObjCLock
{
	@public
	id isa;
	mutex_t lock;
}
@end
@implementation __ObjCLock
+ (id)new
{
	__ObjCLock *l = calloc(1, class_getInstanceSize(self));
	l->isa = self;
	INIT_LOCK(l->lock);
	return l;
}
- (id)retain
{
	return self;
}
- (void)release
{
	DESTROY_LOCK(&lock);
	free(self);
}
@end

static char key;

// TODO: This should probably have a special case for classes conforming to the
// NSLocking protocol, just sending them a -lock message.
int objc_sync_enter(id obj)
{
	__ObjCLock *l = objc_getAssociatedObject(obj, &key);
	if (nil == l)
	{
		__ObjCLock *lock = [__ObjCLock new];
		objc_setAssociatedObject(obj, &key, lock, OBJC_ASSOCIATION_RETAIN);
		l = objc_getAssociatedObject(obj, &key);
		// If another thread created the lock while we were doing this, then
		// use their one and free ours
		if (l != lock)
		{
			[lock release];
		}
	}
	LOCK(&l->lock);
	return 0;
}

int objc_sync_exit(id obj)
{
	__ObjCLock *l = objc_getAssociatedObject(obj, &key);
	if (nil != l)
	{
		UNLOCK(&l->lock);
	}
	return 0;
}
