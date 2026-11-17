#include "Test.h"
#include <stdio.h>

BOOL deallocCalled = NO;
static char key;

@interface Observed : Test
@end

@implementation Observed
@end

@interface Associated : Test
@end

@implementation Associated
-(void) dealloc
{
	deallocCalled = YES;
	[super dealloc];
}
@end

int main(int argc, const char * argv[]) {
	Class subclass = objc_allocateClassPair([Observed class], "ObservedSubclass", 0);
	objc_registerClassPair(subclass);

	@autoreleasepool
	{
		Observed *object = [Observed new];
		Associated *value = [Associated new];
		objc_setAssociatedObject(object, &key, value, OBJC_ASSOCIATION_RETAIN);
		[value release];
		assert(objc_getAssociatedObject(object, &key) == value);

		object_setClass(object, subclass);
		assert(object_getClass(object) == subclass);

		fprintf(stderr, "Association after object_setClass: %p (expected %p)\n",
			   objc_getAssociatedObject(object, &key), value);
		assert(objc_getAssociatedObject(object, &key) == value);

		object_setClass(object, [Observed class]);
		fprintf(stderr, "Association after object_setClass: %p (expected %p)\n",
			   objc_getAssociatedObject(object, &key), value);
		assert(objc_getAssociatedObject(object, &key) == value);

		[object release];
	}
	assert(deallocCalled);
	return 0;
}
