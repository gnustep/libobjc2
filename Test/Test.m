#import "../objc/runtime.h"
#import "../objc/objc-arc.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(objc_root_class)
__attribute__((objc_root_class))
#endif
@interface Test { id isa; }
@end
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
