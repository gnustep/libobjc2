#import "../objc/runtime.h"
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
+ (id)class { return self; }
+ (id)new
{
	return class_createInstance(self, 0);
}
- (void)dealloc
{
	object_dispose(self);
}
@end
