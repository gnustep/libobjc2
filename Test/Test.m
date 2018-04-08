#import "../objc/runtime.h"
#import "../objc/objc-arc.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "Test.h"

@implementation NSConstantString @end

@implementation Test
+ (Class)class { return self; }
+ (id)new
{
	return class_createInstance(self, 0);
}
- (void)dealloc
{
	object_dispose(self);
}
- (id)autorelease
{
	return objc_autorelease(self);
}
- (id)retain
{
	return objc_retain(self);
}
- (void)release
{
	objc_release(self);
}
- (void)_ARCCompliantRetainRelease {}
@end

@implementation NSAutoreleasePool
- (void)_ARCCompatibleAutoreleasePool {}
+ (void)addObject:(id)anObject
{
	objc_autorelease(anObject);
}
@end
