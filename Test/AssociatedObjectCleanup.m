#include "Test.h"

// A hidden class carries uninstalled_dtable until the object is next
// messaged, and a method is recorded in the class as it is installed into a
// dtable, so the .cxx_destruct that carries the cleanup has to be recorded
// when the hidden class is made.  An object destroyed without being messaged
// again after gaining an association exercises that.

static BOOL deallocCalled = NO;
static const char *key = "AssociatedObjectCleanupKey";

@interface Associated : Test
@end

@implementation Associated
- (void)dealloc
{
	deallocCalled = YES;
	[super dealloc];
}
@end

int main(void)
{
	Associated *object = [Associated new];
	Test *holder = [Test new];

	objc_setAssociatedObject(holder, &key, object, OBJC_ASSOCIATION_RETAIN);
	// The association holds the only reference.
	[object release];
	assert(!deallocCalled);

	// Destroy the holder without sending it another message first.
	object_dispose(holder);

	assert(deallocCalled);
	return 0;
}
